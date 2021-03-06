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
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <sys/time.h>
#include <time.h>
#include "srslte/srslte.h"

uint32_t long_cb = 0;

void usage(char *prog) {
  printf("Usage: %s\n", prog);
  printf("\t-l long_cb [Default check all]\n", long_cb);
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "lv")) != -1) {
    switch (opt) {
    case 'l':
      long_cb = atoi(argv[optind]);
      break;
    case 'v':
      srslte_verbose++;
      break;
    default:
      usage(argv[0]);
      exit(-1);
    }
  }
}

uint8_t input_bytes[6144/8];
uint8_t input_bits[6144];
uint8_t output_bits[3*6144+12];
uint8_t output_bytes[3*6144+12];
uint8_t output_bits2[3*6144+12];
uint8_t output_bits3[3*6144+12];

int main(int argc, char **argv) {

  parse_args(argc, argv);
  
  srslte_tcod_gentable();

  srslte_tcod_t tcod;
  srslte_tcod_init(&tcod, 6144);

  uint32_t st=0, end=187;
  if (long_cb) {
    st=srslte_cbsegm_cbindex(long_cb);
    end=st;
  }
  
  for (uint32_t len=st;len<=end;len++) {
    long_cb = srslte_cbsegm_cbsize(len); 
    printf("Checking long_cb=%d\n", long_cb);
    for (int i=0;i<long_cb/8;i++) {
      input_bytes[i] = rand()%256;
    }
    
    srslte_bit_unpack_vector(input_bytes, input_bits, long_cb);

    if (SRSLTE_VERBOSE_ISINFO()) {
      printf("Input bits:\n");
      for (int i=0;i<long_cb/8;i++) {
        srslte_vec_fprint_b(stdout, &input_bits[i*8], 8);
      }
    }

    srslte_tcod_encode(&tcod, input_bits, output_bits, long_cb);
    srslte_tcod_encode_lut(&tcod, input_bytes, output_bytes, long_cb);
    
    srslte_bit_unpack_vector(output_bytes, output_bits2, 2*long_cb+12);

    /* de-Interleace bits for comparison */
    for (int i=0;i<long_cb;i++) {
      for (int j=0;j<2;j++) {
        output_bits3[j*long_cb+i] = output_bits[3*i+j+1]; 
      }
    }
    // copy tail 
    memcpy(&output_bits3[2*long_cb], &output_bits[3*long_cb], 12);

    if (SRSLTE_VERBOSE_ISINFO()) {
      printf("1st encoder\n");
      srslte_vec_fprint_b(stdout, output_bits2, long_cb); 
      srslte_vec_fprint_b(stdout, output_bits3, long_cb); 
      
      printf("2nd encoder\n");
      srslte_vec_fprint_b(stdout, &output_bits2[long_cb], long_cb); 
      srslte_vec_fprint_b(stdout, &output_bits3[long_cb], long_cb); 

      printf("tail\n");
      srslte_vec_fprint_b(stdout, &output_bits2[2*long_cb], 12); 
      srslte_vec_fprint_b(stdout, &output_bits3[2*long_cb], 12); 
      printf("\n");
    }  
    for (int i=0;i<2*long_cb+12;i++) {
      if (output_bits2[i] != output_bits3[i]) {
        printf("error in bit %d, len=%d\n", i, len);
        exit(-1);
      }
    }
  }
  
  srslte_tcod_free(&tcod);
  printf("Done\n");
  exit(0);
}
