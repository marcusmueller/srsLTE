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


#include <uhd/usrp/multi_usrp.hpp>
#include <pthread.h>

class cuhd_handler {
public:
  uhd::usrp::multi_usrp::sptr usrp;
  uhd::rx_streamer::sptr rx_stream;
  bool rx_stream_enable;
  uhd::tx_streamer::sptr tx_stream;
  
  // The following variables are for threaded RX gain control 
  pthread_t thread_gain; 
  pthread_cond_t  cond; 
  pthread_mutex_t mutex; 
  double cur_rx_gain; 
  double new_rx_gain;   
  bool   tx_gain_same_rx; 
  float  tx_rx_gain_offset; 
  uhd::gain_range_t rx_gain_range; 
  size_t rx_nof_samples;
  size_t tx_nof_samples;
  double tx_rate;
};
