/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 The srsLTE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "srslte/fec/cbsegm.h"
#include "srslte/fec/turbocoder.h"
#include "srslte/utils/bit.h"
#include "srslte/utils/vector.h"

#define NOF_REGS 3

#define RATE 3
#define TOTALTAIL 12

uint8_t tcod_lut_next_state[188][8][256];
uint8_t tcod_lut_output[188][8][256];
uint16_t tcod_per_fw[188][6114];

int srslte_tcod_init(srslte_tcod_t *h, uint32_t max_long_cb) {

  if (srslte_tc_interl_init(&h->interl, max_long_cb)) {
    return -1;
  }
  h->max_long_cb = max_long_cb;
  h->temp = srslte_vec_malloc(max_long_cb/8);
  return 0;
}

void srslte_tcod_free(srslte_tcod_t *h) {
  srslte_tc_interl_free(&h->interl);
  h->max_long_cb = 0;
  if (h->temp) {
    free(h->temp);
  }
}

/* Expects bits (1 byte = 1 bit) and produces bits. The systematic and parity bits are interlaced in the output */
int srslte_tcod_encode(srslte_tcod_t *h, uint8_t *input, uint8_t *output, uint32_t long_cb) 
{

  uint8_t reg1_0, reg1_1, reg1_2, reg2_0, reg2_1, reg2_2;
  uint32_t i, k = 0, j;
  uint8_t bit;
  uint8_t in, out;
  uint32_t *per;

  if (long_cb > h->max_long_cb) {
    fprintf(stderr, "Turbo coder initiated for max_long_cb=%d\n",
        h->max_long_cb);
    return -1;
  }

  if (srslte_tc_interl_LTE_gen(&h->interl, long_cb)) {
    fprintf(stderr, "Error initiating TC interleaver\n");
    return -1;
  }

  per = h->interl.forward;

  reg1_0 = 0;
  reg1_1 = 0;
  reg1_2 = 0;

  reg2_0 = 0;
  reg2_1 = 0;
  reg2_2 = 0;

  k = 0;
  for (i = 0; i < long_cb; i++) {
    if (input[i] == SRSLTE_TX_NULL) {
      bit = 0;
    } else {
      bit = input[i];
    }
    output[k] = input[i];
    
    k++;

    in = bit ^ (reg1_2 ^ reg1_1);
    out = reg1_2 ^ (reg1_0 ^ in);

    reg1_2 = reg1_1;
    reg1_1 = reg1_0;
    reg1_0 = in;
    
    if (input[i] == SRSLTE_TX_NULL) {
      output[k] = SRSLTE_TX_NULL;
    } else {
      output[k] = out;
    }
    k++;

    bit = input[per[i]];
    if (bit == SRSLTE_TX_NULL) {
      bit = 0; 
    }

    in = bit ^ (reg2_2 ^ reg2_1);
    out = reg2_2 ^ (reg2_0 ^ in);

    reg2_2 = reg2_1;
    reg2_1 = reg2_0;
    reg2_0 = in;

    output[k] = out;
    k++;

  }

  k = 3 * long_cb;

  /* TAILING CODER #1 */
  for (j = 0; j < NOF_REGS; j++) {
    bit = reg1_2 ^ reg1_1;

    output[k] = bit;
    k++;

    in = bit ^ (reg1_2 ^ reg1_1);
    out = reg1_2 ^ (reg1_0 ^ in);

    reg1_2 = reg1_1;
    reg1_1 = reg1_0;
    reg1_0 = in;

    output[k] = out;
    k++;
  }

  /* TAILING CODER #2 */
  for (j = 0; j < NOF_REGS; j++) {
    bit = reg2_2 ^ reg2_1;

    output[k] = bit;
    k++;

    in = bit ^ (reg2_2 ^ reg2_1);
    out = reg2_2 ^ (reg2_0 ^ in);

    reg2_2 = reg2_1;
    reg2_1 = reg2_0;
    reg2_0 = in;

    output[k] = out;
    k++;
  }
  return 0;
}

/* Expects bytes and produces bytes. The systematic and parity bits are interlaced in the output */
int srslte_tcod_encode_lut(srslte_tcod_t *h, uint8_t *input, uint8_t *output, uint32_t long_cb) 
{
  if (long_cb % 8) {
    fprintf(stderr, "Turbo coder LUT implementation long_cb must be multiple of 8\n");
    return -1; 
  }
  
  int ret = srslte_cbsegm_cbindex(long_cb);
  if (ret < 0) {
    return -1;
  }
  uint8_t len_idx = (uint8_t) ret; 
    
  /* Parity bits for the 1st constituent encoders */
  uint8_t state0 = 0;   
  for (uint32_t i=0;i<long_cb/8;i++) {
    output[i] = tcod_lut_output[len_idx][state0][input[i]];    
    state0 = tcod_lut_next_state[len_idx][state0][input[i]] % 8;
  }

  /* Interleave input */  
  for (uint32_t i=0;i<long_cb/8;i++) {
    h->temp[i] = 0; 
    for (uint32_t j=0;j<8;j++) {
      uint32_t i_p = tcod_per_fw[len_idx][i*8+j];      
      if (input[i_p/8] & (1<<(7-i_p%8))) {
        h->temp[i] |= 1<<(7-j);
      }
    }
  }

  /* Parity bits for the 2nd constituent encoders */
  uint8_t state1 = 0;
  for (uint32_t i=0;i<long_cb/8;i++) {
    output[long_cb/8+i] = tcod_lut_output[len_idx][state1][h->temp[i]];    
    state1 = tcod_lut_next_state[len_idx][state1][h->temp[i]] % 8;
  }

  /* Tail bits */
  uint8_t reg1_0, reg1_1, reg1_2, reg2_0, reg2_1, reg2_2;
  uint8_t bit, in, out; 
  uint8_t k=0;
  uint8_t tail[12];
  
  reg2_0 = (state1&4)>>2;
  reg2_1 = (state1&2)>>1;
  reg2_2 = state1&1;
  
  reg1_0 = (state0&4)>>2;
  reg1_1 = (state0&2)>>1;
  reg1_2 = state0&1;
    
  /* TAILING CODER #1 */
  for (uint32_t j = 0; j < NOF_REGS; j++) {
    bit = reg1_2 ^ reg1_1;

    tail[k] = bit;
    k++;

    in = bit ^ (reg1_2 ^ reg1_1);
    out = reg1_2 ^ (reg1_0 ^ in);

    reg1_2 = reg1_1;
    reg1_1 = reg1_0;
    reg1_0 = in;

    tail[k] = out;
    k++;
  }

  /* TAILING CODER #2 */
  for (uint32_t j = 0; j < NOF_REGS; j++) {
    bit = reg2_2 ^ reg2_1;

    tail[k] = bit;
    k++;

    in = bit ^ (reg2_2 ^ reg2_1);
    out = reg2_2 ^ (reg2_0 ^ in);

    reg2_2 = reg2_1;
    reg2_1 = reg2_0;
    reg2_0 = in;

    tail[k] = out;
    k++;
  }
  
  srslte_bit_pack_vector(tail, &output[2*(long_cb/8)], TOTALTAIL);
  
  return 2*long_cb+TOTALTAIL;
}

void srslte_tcod_gentable() {
  srslte_tc_interl_t interl; 

  if (srslte_tc_interl_init(&interl, 6144)) {
    fprintf(stderr, "Error initiating interleave\n");
    return;
  }
  
  for (uint32_t len=0;len<188;len++) {
    uint32_t long_cb = srslte_cbsegm_cbsize(len);
    if (srslte_tc_interl_LTE_gen(&interl, long_cb)) {
      fprintf(stderr, "Error initiating TC interleaver for long_cb=%d\n", long_cb);
      return;
    }
    // Save fw/bw permutation tables
    for (uint32_t i=0;i<long_cb;i++) {
      tcod_per_fw[len][i] = interl.forward[i];
    }
    for (uint32_t i=long_cb;i<6144;i++) {
      tcod_per_fw[len][i] = 0;
    }
    // Compute state transitions
    for (uint32_t state=0;state<8;state++) {
      for (uint32_t data=0;data<256;data++) {
          
        uint8_t reg_0, reg_1, reg_2;
        reg_0 = (state&4)>>2;
        reg_1 = (state&2)>>1;
        reg_2 = state&1;
        
        tcod_lut_output[len][state][data] = 0;
        uint8_t bit, in, out; 
        for (uint32_t i = 0; i < 8; i++) {
          bit = (data&(1<<(7-i)))?1:0;

          in = bit ^ (reg_2 ^ reg_1);
          out = reg_2 ^ (reg_0 ^ in);

          reg_2 = reg_1;
          reg_1 = reg_0;
          reg_0 = in;

          tcod_lut_output[len][state][data] |= out<<(7-i);

        }
        tcod_lut_next_state[len][state][data] = reg_0<<2 | reg_1<<1 | reg_2;
      }
    }  
  }

  srslte_tc_interl_free(&interl);
}
