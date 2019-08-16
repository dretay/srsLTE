/*
 * Copyright 2013-2019 Software Radio Systems Limited
 *
 * This file is a modified version of pdsch_enodeb from srsLTE.
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
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include "pdcch_enodeb.h"

static bool go_exit = false;
void sig_int_handler(int signo) {
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  }
}

int main(int argc, char **argv) {
  PDCCH_ENODEB base;
  base.run(argc, argv);
}

SearchSpace::SearchSpace(uint16_t rnti, uint32_t sf_idx, uint32_t ncce) {
  srslte_dci_location_t loc[30];
  uint32_t size = srslte_pdcch_ue_locations_ncce(ncce, loc, 30, sf_idx, rnti);
  locations = std::vector<srslte_dci_location_t>(loc, loc + size);
  this->rnti = rnti;
}

SearchSpace::~SearchSpace() {

}

uint16_t SearchSpace::getRNTI() {
  return rnti;
}

std::vector<srslte_dci_location_t>& SearchSpace::getLocations() {
  return locations;
}


PDCCH_ENODEB::PDCCH_ENODEB() :
  fileReplay(cell)
{
  for(uint32_t cfi=0; cfi<3; cfi++) {
    for(uint32_t sf_idx=0; sf_idx<10; sf_idx++) {
      searchSpace[cfi][sf_idx] = nullptr;
    }
  }
}

PDCCH_ENODEB::~PDCCH_ENODEB() {
  for(uint32_t cfi=0; cfi<3; cfi++) {
    for(uint32_t sf_idx=0; sf_idx<10; sf_idx++) {
      delete searchSpace[cfi][sf_idx];
      searchSpace[cfi][sf_idx] = nullptr;
    }
  }
}

void PDCCH_ENODEB::usage(char *prog) {
  printf("Usage: %s [iIagmfoncvpuxb]\n", prog);
  printf("\t-i input_file for DCI\n");
#ifndef DISABLE_RF
  printf("\t-I RF device [Default %s]\n", rf_dev);
  printf("\t-a RF args [Default %s]\n", rf_args);
  printf("\t-l RF amplitude [Default %.2f]\n", static_cast<double>(rf_amp));
  printf("\t-g RF TX gain [Default %.2f dB]\n", static_cast<double>(rf_gain));
  printf("\t-f RF TX frequency [Default %.1f MHz]\n", static_cast<double>(rf_freq) / 1000000);
#else
  printf("\t   RF is disabled.\n");
#endif
  printf("\t-o output_file [Default use RF board]\n");
  printf("\t-m MCS index [Default %d]\n", mcs_idx);
  printf("\t-n number of frames [Default %d]\n", nof_frames);
  printf("\t-c cell id [Default %d]\n", cell.id);
  printf("\t-p nof_prb [Default %d]\n", cell.nof_prb);
  printf("\t-M MBSFN area id [Default %d]\n", mbsfn_area_id);
  printf("\t-x Transmission mode [1-4] [Default %d]\n", transmission_mode + 1);
  printf("\t-b Precoding Matrix Index (multiplex mode only)* [Default %d]\n", multiplex_pmi);
  printf("\t-w Number of codewords/layers (multiplex mode only)* [Default %d]\n", multiplex_nof_layers);
  //printf("\t-u listen TCP/UDP port for input data (if mbsfn is active then the stream is over mbsfn only) (-1 is random) [Default %d]\n", net_port);
  printf("\t-v [set srslte_verbose to debug, default none]\n");
  printf("\t-s output file SNR [Default %f]\n", static_cast<double>(output_file_snr));
  printf("\t-q Enable/Disable 256QAM modulation (default %s)\n", enable_256qam ? "enabled" : "disabled");
  printf("\n");
  printf("\t*: See 3GPP 36.212 Table  5.3.3.1.5-4 for more information\n");
}

void PDCCH_ENODEB::parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "iIadglfmoncpqvutxbwMsB")) != -1) {

    switch (opt) {
      case 'I':
        rf_dev = argv[optind];
        break;
      case 'a':
        rf_args = argv[optind];
        break;
      case 'g':
        rf_gain = (float)atof(argv[optind]);
        break;
      case 'l':
        rf_amp = (float)atof(argv[optind]);
        break;
      case 'f':
        rf_freq = (float)atof(argv[optind]);
        break;
      case 'i':
        input_file_name = argv[optind];
        break;
      case 'o':
        output_file_name = argv[optind];
        break;
      case 'm':
        mcs_idx = (uint32_t)atoi(argv[optind]);
        break;
//      case 'u':
//        net_port = atoi(argv[optind]);
//        break;
      case 'n':
        nof_frames = atoi(argv[optind]);
        break;
      case 'p':
        cell.nof_prb = (uint32_t)atoi(argv[optind]);
        break;
      case 'c':
        cell.id = (uint32_t)atoi(argv[optind]);
        break;
      case 'x':
        transmission_mode = (srslte_tm_t)(atoi(argv[optind]) - 1);
        break;
      case 'b':
        multiplex_pmi = (uint32_t) atoi(argv[optind]);
        break;
      case 'w':
        multiplex_nof_layers = (uint32_t) atoi(argv[optind]);
        break;
      case 'M':
        mbsfn_area_id = atoi(argv[optind]);
        break;
      case 'v':
        srslte_verbose++;
        break;
      case 's':
        output_file_snr = (float)atof(argv[optind]);
        break;
      case 'B':
        mbsfn_sf_mask = (uint8_t)atoi(argv[optind]);
        break;
      case 'q':
        enable_256qam ^= true;
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
#ifdef DISABLE_RF
  if (!output_file_name) {
    usage(argv[0]);
    exit(-1);
  }
#endif
}

void PDCCH_ENODEB::base_init() {
  uint32_t i;

  if(input_file_name) {
    fileReplay.openFile(input_file_name);
  }

  /* Configure cell and PDSCH in function of the transmission mode */
  switch (transmission_mode) {
    case SRSLTE_TM1:
      cell.nof_ports = 1;
      break;
    case SRSLTE_TM2:
    case SRSLTE_TM3:
    case SRSLTE_TM4:
      cell.nof_ports = 2;
      break;
    default:
      ERROR("Transmission mode %d not implemented or invalid\n", transmission_mode);
      exit(-1);
  }

  /* Allocate memory */
  for(i = 0; i < SRSLTE_MAX_CODEWORDS; i++) {
    data[i] = static_cast<uint8_t*>(srslte_vec_malloc(sizeof(uint8_t) * SOFTBUFFER_SIZE));
    if (!data[i]) {
      perror("malloc");
      exit(-1);
    }
    bzero(data[i], sizeof(uint8_t) * SOFTBUFFER_SIZE);
  }
  data_mbms = static_cast<uint8_t*>(srslte_vec_malloc(sizeof(uint8_t) * SOFTBUFFER_SIZE));


  /* init memory */
  for (i = 0; i < SRSLTE_MAX_PORTS; i++) {
    sf_buffer[i] = static_cast<cf_t*>(srslte_vec_malloc(sizeof(cf_t) * (uint32_t)sf_n_re));
    if (!sf_buffer[i]) {
      perror("malloc");
      exit(-1);
    }
  }

  for (i = 0; i < SRSLTE_MAX_PORTS; i++) {
    output_buffer[i] = static_cast<cf_t*>(srslte_vec_malloc(sizeof(cf_t) * (uint32_t)sf_n_samples));
    if (!output_buffer[i]) {
      perror("malloc");
      exit(-1);
    }
    bzero(output_buffer[i], sizeof(cf_t) * (uint32_t)sf_n_samples);
  }


  /* open file or USRP */
  if (output_file_name) {
    if (strcmp(output_file_name, "NULL")) {
      if (srslte_filesink_init(&fsink, output_file_name, SRSLTE_COMPLEX_FLOAT_BIN)) {
        ERROR("Error opening file %s\n", output_file_name);
        exit(-1);
      }
      null_file_sink = false;
    } else {
      null_file_sink = true;
    }
  } else {
#ifndef DISABLE_RF
    printf("Opening RF device...\n");
    if (srslte_rf_open_devname(&rf, rf_dev, rf_args, cell.nof_ports)) {
      fprintf(stderr, "Error opening rf\n");
      exit(-1);
    }
#else
    printf("Error RF not available. Select an output file\n");
    exit(-1);
#endif
  }

//  if (net_port > 0) {
//    if (srslte_netsource_init(&net_source, "127.0.0.1", (uint16_t)net_port, SRSLTE_NETSOURCE_UDP)) {
//      ERROR("Error creating input UDP socket at port %d\n", net_port);
//      exit(-1);
//    }
//    if (null_file_sink) {
//      if (srslte_netsink_init(&net_sink, "127.0.0.1", (uint16_t)net_port+1, SRSLTE_NETSINK_TCP)) {
//        ERROR("Error sink\n");
//        exit(-1);
//      }
//    }
//    if (sem_init(&net_sem, 0, 1)) {
//      perror("sem_init");
//      exit(-1);
//    }
//  }

  /* create ifft object */
  for (i = 0; i < cell.nof_ports; i++) {
    if (srslte_ofdm_tx_init(&ifft[i], SRSLTE_CP_NORM, sf_buffer[i], output_buffer[i], cell.nof_prb)) {
      ERROR("Error creating iFFT object\n");
      exit(-1);
    }

    srslte_ofdm_set_normalize(&ifft[i], true);
  }

  if (srslte_ofdm_tx_init_mbsfn(&ifft_mbsfn, SRSLTE_CP_EXT, sf_buffer[0], output_buffer[0], cell.nof_prb)) {
    ERROR("Error creating iFFT object\n");
    exit(-1);
  }
  srslte_ofdm_set_non_mbsfn_region(&ifft_mbsfn, 2);
  srslte_ofdm_set_normalize(&ifft_mbsfn, true);

  if (srslte_pbch_init(&pbch)) {
    ERROR("Error creating PBCH object\n");
    exit(-1);
  }
  if (srslte_pbch_set_cell(&pbch, cell)) {
    ERROR("Error creating PBCH object\n");
    exit(-1);
  }




  if (srslte_regs_init(&regs, cell)) {
    ERROR("Error initiating regs\n");
    exit(-1);
  }
  if (srslte_pcfich_init(&pcfich, 1)) {
    ERROR("Error creating PBCH object\n");
    exit(-1);
  }
  if (srslte_pcfich_set_cell(&pcfich, &regs, cell)) {
    ERROR("Error creating PBCH object\n");
    exit(-1);
  }

  if (srslte_pdcch_init_enb(&pdcch, cell.nof_prb)) {
    ERROR("Error creating PDCCH object\n");
    exit(-1);
  }
  if (srslte_pdcch_set_cell(&pdcch, &regs, cell)) {
    ERROR("Error creating PDCCH object\n");
    exit(-1);
  }

  if (srslte_pdsch_init_enb(&pdsch, cell.nof_prb)) {
    ERROR("Error creating PDSCH object\n");
    exit(-1);
  }
  if (srslte_pdsch_set_cell(&pdsch, cell)) {
    ERROR("Error creating PDSCH object\n");
    exit(-1);
  }

  srslte_pdsch_set_rnti(&pdsch, UE_CRNTI);


  if(mbsfn_area_id > -1){
    if (srslte_pmch_init(&pmch, cell.nof_prb, 1)) {
      ERROR("Error creating PMCH object\n");
    }
    srslte_pmch_set_area_id(&pmch, mbsfn_area_id);
  }

  for (i = 0; i < SRSLTE_MAX_CODEWORDS; i++) {
    softbuffers[i] = static_cast<srslte_softbuffer_tx_t*>(calloc(sizeof(srslte_softbuffer_tx_t), 1));
    if (!softbuffers[i]) {
      ERROR("Error allocating soft buffer\n");
      exit(-1);
    }

    if (srslte_softbuffer_tx_init(softbuffers[i], cell.nof_prb)) {
      ERROR("Error initiating soft buffer\n");
      exit(-1);
    }
  }
}


void PDCCH_ENODEB::base_free() {
  uint32_t i;
  for (i = 0; i < SRSLTE_MAX_CODEWORDS; i++) {
    srslte_softbuffer_tx_free(softbuffers[i]);
    if (softbuffers[i]) {
      free(softbuffers[i]);
    }
  }
  srslte_pdsch_free(&pdsch);
  srslte_pdcch_free(&pdcch);
  srslte_regs_free(&regs);
  srslte_pbch_free(&pbch);
  if(mbsfn_area_id > -1){
    srslte_pmch_free(&pmch);
  }
  srslte_ofdm_tx_free(&ifft_mbsfn);
  for (i = 0; i < cell.nof_ports; i++) {
    srslte_ofdm_tx_free(&ifft[i]);
  }

  for (i = 0; i < SRSLTE_MAX_CODEWORDS; i++) {
    if (data[i]) {
      free(data[i]);
    }
  }

  for (i = 0; i < SRSLTE_MAX_PORTS; i++) {
    if (sf_buffer[i]) {
      free(sf_buffer[i]);
    }

    if (output_buffer[i]) {
      free(output_buffer[i]);
    }
  }
  if (output_file_name) {
    if (!null_file_sink) {
      srslte_filesink_free(&fsink);
    }
  } else {
#ifndef DISABLE_RF
    srslte_rf_close(&rf);
#endif
  }

//  if (net_port > 0) {
//    srslte_netsource_free(&net_source);
//    sem_close(&net_sem);
//  }
}


unsigned int PDCCH_ENODEB::reverse(unsigned int x) {
  x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
  x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
  x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
  x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
  return((x >> 16) | (x << 16));

}

uint32_t PDCCH_ENODEB::prbset_to_bitmask() {
  uint32_t mask=0;
  int nb = (int)(ceilf((float) cell.nof_prb / srslte_ra_type0_P(cell.nof_prb)));
  for (int i=0;i<nb;i++) {
    if (i >= prbset_orig && i < prbset_orig + prbset_num) {
      mask = mask | (0x1<<i);
    }
  }
  return reverse(mask)>>(32-nb);
}

//int PDCCH_ENODEB::update_radl() {

//  ZERO_OBJECT(dci_dl);

//  /* Configure cell and PDSCH in function of the transmission mode */
//  switch (transmission_mode) {
//    case SRSLTE_TM1:
//    case SRSLTE_TM2:
//      nof_tb = 1;
//      dci_dl.format = SRSLTE_DCI_FORMAT1;
//      break;
//    case SRSLTE_TM3:
//      dci_dl.format = SRSLTE_DCI_FORMAT2A;
//      nof_tb = 2;
//      break;
//    case SRSLTE_TM4:
//      dci_dl.format = SRSLTE_DCI_FORMAT2;
//      nof_tb = multiplex_nof_layers;
//      if (multiplex_nof_layers == 1) {
//        dci_dl.pinfo = (uint8_t)(multiplex_pmi + 1);
//      } else {
//        dci_dl.pinfo = (uint8_t)multiplex_pmi;
//      }
//      break;
//    default:
//      ERROR("Transmission mode not implemented.");
//      exit(-1);
//  }

//  dci_dl.rnti                    = UE_CRNTI;
//  dci_dl.pid                     = 0;
//  dci_dl.tb[0].mcs_idx           = mcs_idx;
//  dci_dl.tb[0].ndi               = 0;
//  dci_dl.tb[0].rv                = rvidx[0];
//  dci_dl.tb[0].cw_idx            = 0;
//  dci_dl.alloc_type              = SRSLTE_RA_ALLOC_TYPE0;
//  dci_dl.type0_alloc.rbg_bitmask = prbset_to_bitmask();

//  if (nof_tb > 1) {
//    dci_dl.tb[1].mcs_idx = mcs_idx;
//    dci_dl.tb[1].ndi     = 0;
//    dci_dl.tb[1].rv      = rvidx[1];
//    dci_dl.tb[1].cw_idx  = 1;
//  } else {
//    SRSLTE_DCI_TB_DISABLE(dci_dl.tb[1]);
//  }

//  srslte_dci_dl_fprint(stdout, &dci_dl, cell.nof_prb);
//  if (transmission_mode != SRSLTE_TM1) {
//    printf("\nTransmission mode key table:\n");
//    printf("   Mode   |   1TB   | 2TB |\n");
//    printf("----------+---------+-----+\n");
//    printf("Diversity |    x    |     |\n");
//    printf("      CDD |         |  z  |\n");
//    printf("Multiplex | q,w,e,r | a,s |\n");
//    printf("\n");
//    printf("Type new MCS index (0-28) or mode key and press Enter: ");
//  } else {
//    printf("Type new MCS index (0-28) and press Enter: ");
//  }
//  fflush(stdout);

//  return 0;
//}

/* Read new MCS from stdin */
//int PDCCH_ENODEB::update_control() {
//  char input[128];

//  fd_set set;
//  FD_ZERO(&set);
//  FD_SET(0, &set);

//  struct timeval to;
//  to.tv_sec = 0;
//  to.tv_usec = 0;

//  int n = select(1, &set, nullptr, nullptr, &to);
//  if (n == 1) {
//    // stdin ready
//    if (fgets(input, sizeof(input), stdin)) {
//      if(input[0] == 27) {
//        switch(input[2]) {
//          case RIGHT_KEY:
//            if (prbset_orig  + prbset_num < (int)(ceilf((float) cell.nof_prb / srslte_ra_type0_P(cell.nof_prb))))
//              prbset_orig++;
//            break;
//          case LEFT_KEY:
//            if (prbset_orig > 0)
//              prbset_orig--;
//            break;
//          case UP_KEY:
//            if (prbset_num < (int)(ceilf((float)cell.nof_prb / srslte_ra_type0_P(cell.nof_prb))))
//              prbset_num++;
//            break;
//          case DOWN_KEY:
//            last_prbset_num = prbset_num;
//            if (prbset_num > 0)
//              prbset_num--;
//            break;
//        }
//      } else {
//        switch (input[0]) {
//          case 'q':
//            transmission_mode    = SRSLTE_TM4;
//            multiplex_pmi        = 0;
//            multiplex_nof_layers = 1;
//            break;
//          case 'w':
//            transmission_mode    = SRSLTE_TM4;
//            multiplex_pmi        = 1;
//            multiplex_nof_layers = 1;
//            break;
//          case 'e':
//            transmission_mode    = SRSLTE_TM4;
//            multiplex_pmi        = 2;
//            multiplex_nof_layers = 1;
//            break;
//          case 'r':
//            transmission_mode    = SRSLTE_TM4;
//            multiplex_pmi        = 3;
//            multiplex_nof_layers = 1;
//            break;
//          case 'a':
//            transmission_mode    = SRSLTE_TM4;
//            multiplex_pmi        = 0;
//            multiplex_nof_layers = 2;
//            break;
//          case 's':
//            transmission_mode    = SRSLTE_TM4;
//            multiplex_pmi        = 1;
//            multiplex_nof_layers = 2;
//            break;
//          case 'z':
//            transmission_mode = SRSLTE_TM3;
//            break;
//          case 'x':
//            transmission_mode = SRSLTE_TM2;
//            break;
//          default:
//            last_mcs_idx = mcs_idx;
//            mcs_idx = (uint32_t)atoi(input);
//        }
//      }
//      bzero(input,sizeof(input));
//      if (update_radl()) {
//        printf("Trying with last known MCS index\n");
//        mcs_idx = last_mcs_idx;
//        prbset_num = last_prbset_num;
//        return update_radl();
//      }
//    }
//    return 0;
//  } else if (n < 0) {
//    // error
//    perror("select");
//    return -1;
//  } else {
//    return 0;
//  }
//}

void PDCCH_ENODEB::updateSearchSpace() {
  for(uint32_t cfi=0; cfi<3; cfi++) {
    for(uint32_t sf_idx=0; sf_idx<10; sf_idx++) {
      delete searchSpace[cfi][sf_idx];
      uint32_t ncce = pdcch.nof_cce[cfi];
      searchSpace[cfi][sf_idx] = new SearchSpace(UE_CRNTI, sf_idx, ncce);
    }
  }
}


/* Function run in a separate thread to receive UDP data */
//void* PDCCH_ENODEB::net_thread_fnc(void *arg) {
//  return static_cast<PDCCH_ENODEB*>(arg)->_net_thread_fnc();
//}

//void* PDCCH_ENODEB::_net_thread_fnc() {

//  int n;
//  int rpm = 0, wpm=0;

//  do {
//    n = srslte_netsource_read(&net_source, &data2[rpm], DATA_BUFF_SZ-rpm);
//    if (n > 0) {
//      // FIXME: I assume that both transport blocks have same size in case of 2 tb are active

//      int nbytes = 1 + (((mbsfn_area_id > -1) ? (pmch_cfg.pdsch_cfg.grant.tb[0].tbs)
//                        : (pdsch_cfg.grant.tb[0].tbs + pdsch_cfg.grant.tb[1].tbs)) -
//          1) /
//          8;
//      rpm += n;
//      INFO("received %d bytes. rpm=%d/%d\n",n,rpm,nbytes);
//      wpm = 0;
//      while (rpm >= nbytes) {
//        // wait for packet to be transmitted
//        sem_wait(&net_sem);
//        if(mbsfn_area_id > -1){
//          memcpy(data_mbms, &data2[wpm], (size_t)nbytes);
//        }
//        else{
//          memcpy(data[0], &data2[wpm], (size_t)nbytes / (size_t) 2);
//          memcpy(data[1], &data2[wpm], (size_t)nbytes / (size_t) 2);
//        }
//        INFO("Sent %d/%d bytes ready\n", nbytes, rpm);
//        rpm -= nbytes;
//        wpm += nbytes;
//        net_packet_ready = true;
//      }
//      if (wpm > 0) {
//        INFO("%d bytes left in buffer for next packet\n", rpm);
//        memcpy(data2, &data2[wpm], (size_t)rpm * sizeof(uint8_t));
//      }
//    } else if (n == 0) {
//      rpm = 0;
//    } else {
//      ERROR("Error receiving from network\n");
//      exit(-1);
//    }
//  } while(n >= 0);
//  return nullptr;
//}

int PDCCH_ENODEB::run(int argc, char **argv) {
  int nf=0, N_id_2=0;
  cf_t pss_signal[SRSLTE_PSS_LEN];
  float sss_signal0[SRSLTE_SSS_LEN]; // for subframe 0
  float sss_signal5[SRSLTE_SSS_LEN]; // for subframe 5
  uint8_t bch_payload[SRSLTE_BCH_PAYLOAD_LEN];
  uint32_t i;
  cf_t*                 sf_symbols[SRSLTE_MAX_PORTS];
  uint32_t sfn=0, sf_idx=0;
  srslte_refsignal_t csr_refs;
  srslte_refsignal_t mbsfn_refs;

  srslte_debug_handle_crash(argc, argv);

#ifdef DISABLE_RF
  if (argc < 3) {
    usage(argv[0]);
    exit(-1);
  }
#endif

  parse_args(argc, argv);

  uint8_t mch_table[10];
  bzero(&mch_table[0], sizeof(uint8_t)*10);
  if(mbsfn_area_id > -1) {
    generate_mcch_table(mch_table, mbsfn_sf_mask);
  }
  N_id_2 = cell.id % 3;
  sf_n_re = (int)(2 * SRSLTE_CP_NORM_NSYMB * cell.nof_prb * SRSLTE_NRE);
  sf_n_samples = 2 * SRSLTE_SLOT_LEN(srslte_symbol_sz(cell.nof_prb));

  cell.phich_length = SRSLTE_PHICH_NORM;
  cell.phich_resources = SRSLTE_PHICH_R_1;
  sfn = 0;

  prbset_num = (int)(ceilf((float) cell.nof_prb / srslte_ra_type0_P(cell.nof_prb)));
  last_prbset_num = prbset_num;

  /* this *must* be called after setting slot_len_* */
  base_init();

  /* Generate PSS/SSS signals */
  srslte_pss_generate(pss_signal, (uint32_t)N_id_2);
  srslte_sss_generate(sss_signal0, sss_signal5, cell.id);

  /* Generate reference signals */
  if(srslte_refsignal_cs_init(&csr_refs, cell.nof_prb)) {
    ERROR("Error initializing equalizer\n");
    exit(-1);
  }
  if (mbsfn_area_id > -1) {
    if(srslte_refsignal_mbsfn_init(&mbsfn_refs, cell.nof_prb)) {
      ERROR("Error initializing equalizer\n");
      exit(-1);
    }
    if (srslte_refsignal_mbsfn_set_cell(&mbsfn_refs, cell, (uint16_t)mbsfn_area_id)) {
      ERROR("Error initializing MBSFNR signal\n");
      exit(-1);
    }

  }

  if(srslte_refsignal_cs_set_cell(&csr_refs, cell)){
    ERROR("Error setting cell\n");
    exit(-1);
  }


  for (i = 0; i < SRSLTE_MAX_PORTS; i++) {
    sf_symbols[i] = sf_buffer[i%cell.nof_ports];
  }


#ifndef DISABLE_RF


  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigprocmask(SIG_UNBLOCK, &sigset, nullptr);
  signal(SIGINT, sig_int_handler);

  if (!output_file_name) {

    int srate = srslte_sampling_freq_hz(cell.nof_prb);
    if (srate != -1) {
      if (srate < 10e6) {
        srslte_rf_set_master_clock_rate(&rf, 4*srate);
      } else {
        srslte_rf_set_master_clock_rate(&rf, srate);
      }
      printf("Setting sampling rate %.2f MHz\n", (double) srate/1000000);
      double srate_rf = srslte_rf_set_tx_srate(&rf, (double)srate);
      if (srate_rf != (double)srate) {
        ERROR("Could not set sampling rate\n");
        exit(-1);
      }
    } else {
      ERROR("Invalid number of PRB %d\n", cell.nof_prb);
      exit(-1);
    }
    printf("Set TX gain: %.1f dB\n", srslte_rf_set_tx_gain(&rf, (double)rf_gain));
    printf("Set TX freq: %.2f MHz\n", srslte_rf_set_tx_freq(&rf, cell.nof_ports, (double)rf_freq) / 1000000);
  }
#endif

//  if (update_radl()) {
//    exit(-1);
//  }

//  if (net_port > 0) {
//    if (pthread_create(&net_thread, NULL, net_thread_fnc, NULL)) {
//      perror("pthread_create");
//      exit(-1);
//    }
//  }
  pmch_cfg.pdsch_cfg.grant.tb[0].tbs = 1096;

  srslte_dl_sf_cfg_t dl_sf;
  ZERO_OBJECT(dl_sf);

  updateSearchSpace();

  // Access the DCI factory
  DCIFactory& dciFactory = fileReplay.getFactory();
  bool ret = dciFactory.nextTTI();
  if(!ret) {
    ERROR("Failed to read first TTI from DCI factory");
    exit(-1);
  }

  // Set frame and subframe
  uint32_t tti = dciFactory.getTTI();
  sfn = tti / 10;
  sf_idx = 0;
//  sf_idx = tti % 10;
  nf = 0;

  bool send_data = false;
  for (i = 0; i < SRSLTE_MAX_CODEWORDS; i++) {
    srslte_softbuffer_tx_reset(softbuffers[i]);
  }

#ifndef DISABLE_RF
  bool start_of_burst = true;
#endif

  ZERO_OBJECT(pdsch_cfg);
  for (uint32_t i = 0; i < SRSLTE_MAX_CODEWORDS; i++) {
    pdsch_cfg.softbuffers.tx[i] = softbuffers[i];
  }
  pdsch_cfg.rnti = UE_CRNTI;

  pmch_cfg.pdsch_cfg = pdsch_cfg;

  while ((nf < nof_frames || nof_frames == -1) && !go_exit) {
    while(sf_idx < SRSLTE_NOF_SF_X_FRAME && !go_exit) {
      // Set Antenna port resource elements to zero
      bzero(sf_symbols[0], sizeof(cf_t) * (size_t)sf_n_re);

      // Put PSS and SSS if required
      if (sf_idx == 0 || sf_idx == 5) {
        srslte_pss_put_slot(pss_signal, sf_symbols[0], cell.nof_prb, SRSLTE_CP_NORM);
        srslte_sss_put_slot(sf_idx ? sss_signal5 : sss_signal0, sf_symbols[0], cell.nof_prb,
            SRSLTE_CP_NORM);
      }

      // Copy zeros, SSS, PSS into the rest of antenna ports
      for (i = 1; i < cell.nof_ports; i++) {
        memcpy(sf_symbols[i], sf_symbols[0], sizeof(cf_t) * (size_t)sf_n_re);
      }

      // Put reference signals
      if (mch_table[sf_idx] == 1 && mbsfn_area_id > -1) {
        srslte_refsignal_mbsfn_put_sf(cell, 0, csr_refs.pilots[0][sf_idx], mbsfn_refs.pilots[0][sf_idx], sf_symbols[0]);
      } else {
        dl_sf.tti = sfn * 10 + sf_idx;
        for (i = 0; i < cell.nof_ports; i++) {
          srslte_refsignal_cs_put_sf(&csr_refs, &dl_sf, (uint32_t)i, sf_symbols[i]);
        }
      }

      // Put MIB
      srslte_pbch_mib_pack(&cell, sfn, bch_payload);
      if (sf_idx == 0) {
        srslte_pbch_encode(&pbch, bch_payload, sf_symbols, sfn);
      }

      // Prepare PCFICH
      dl_sf.tti = sfn * 10 + sf_idx;
      if(dl_sf.tti == dciFactory.getTTI()) {
        dl_sf.cfi = dciFactory.getCFI();
      }
      else {
        dl_sf.cfi = cfi;
      }
//      dl_sf.tti = nf * 10 + sf_idx;
//      dl_sf.cfi = cfi;

      srslte_pcfich_encode(&pcfich, &dl_sf, sf_symbols);

//      /* Update DL resource allocation from control port */
//      if (update_control()) {
//        ERROR("Error updating parameters from control port\n");
//      }

//      /* Transmit PDCCH + PDSCH only when there is data to send */
//      if ((net_port > 0) && (mch_table[sf_idx] == 1 && mbsfn_area_id > -1)) {
//        send_data = net_packet_ready;
//        if (net_packet_ready) {
//          INFO("Transmitting packet from port\n");
//        }
//      }
//      else {
//        INFO("SF: %d, Generating %d random bits\n", sf_idx, pdsch_cfg.grant.tb[0].tbs + pdsch_cfg.grant.tb[1].tbs);
//        for (uint32_t tb = 0; tb < SRSLTE_MAX_CODEWORDS; tb++) {
//          if (pdsch_cfg.grant.tb[tb].enabled) {
//            for (i = 0; i < (uint32_t)pdsch_cfg.grant.tb[tb].tbs / 8; i++) {
//              data[tb][i] = (uint8_t) rand();
//            }
//          }
//        }

//        /* Uncomment this to transmit on sf 0 and 5 only  */
//        if (sf_idx != 0 && sf_idx != 5) {
//          send_data = true;
//        }
//        else {
//          send_data = false;
//        }
//      }
      send_data = true;


      if(dl_sf.tti == dciFactory.getTTI()) {
        putDCIfromFactory(dl_sf, sf_symbols, dciFactory);
      }

//      if (send_data) {
//        encodeSubframeData(dl_sf, sf_symbols, !(mch_table[sf_idx] == 0 || mbsfn_area_id < 0));
//      }

      fftSamples(!(mch_table[sf_idx] == 0 || mbsfn_area_id < 0));
      sendSamples(start_of_burst);

      // advance dciFactory only if its TTI matches current TTI
      if(dl_sf.tti == dciFactory.getTTI()) {
        dciFactory.nextTTI();
      }
      if(dciFactory.ended()) {
        go_exit = true;
      }
      // advance sf_idx in any case
      sf_idx++;
    }
    nf++;
    sfn = (sfn + 1) % 1024;
    sf_idx = 0;
  }

  base_free();

  printf("Done\n");
  exit(0);
}

void PDCCH_ENODEB::putDCIfromFactory(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[SRSLTE_MAX_PORTS], DCIFactory& factory) {
  std::vector<srslte_dci_dl_t> dciDL;
  std::vector<srslte_dci_ul_t> dciUL;
  if(dl_sf.tti != factory.getTTI()) {
    ERROR("Conflicting TTI in dl_sf and DCIFactory");
    return;
  }

  dl_sf.sf_type = SRSLTE_SF_NORM;

  dciDL = factory.getDCI_DL();
  dciUL = factory.getDCI_UL();

  for(std::vector<srslte_dci_dl_t>::iterator it = dciDL.begin(); it != dciDL.end(); ++it) {
    //pack DCI
    INFO("Putting DL DCI to location: n=%d, L=%d\n", it->location.ncce, it->location.L);
    srslte_dci_msg_t dci_msg;
    srslte_dci_msg_pack_pdsch(&cell, &dl_sf, nullptr, &(*it), &dci_msg);

    //put DCI message to PDCCH
    if (srslte_pdcch_encode(&pdcch, &dl_sf, &dci_msg, sf_symbols)) {
      ERROR("Error encoding DCI message\n");
      exit(-1);
    }

    //Fill PDSCH with random data here
  }

  for(std::vector<srslte_dci_ul_t>::iterator it = dciUL.begin(); it != dciUL.end(); ++it) {
    //pack DCI
    INFO("Putting DL DCI to location: n=%d, L=%d\n", it->location.ncce, it->location.L);
    srslte_dci_msg_t dci_msg;
    srslte_dci_msg_pack_pusch(&cell, &dl_sf, nullptr, &(*it), &dci_msg);

    //put DCI message to PDCCH
    if (srslte_pdcch_encode(&pdcch, &dl_sf, &dci_msg, sf_symbols)) {
      ERROR("Error encoding DCI message\n");
      exit(-1);
    }

    //Fill PDSCH with random data here
  }
}

//void PDCCH_ENODEB::encodeSubframeData(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[SRSLTE_MAX_PORTS], bool mbsfn) {
//  if (!mbsfn) {
//    encodeSubframeDataNormal(dl_sf, sf_symbols);
//  }
//  else {
//    encodeSubframeDataMBSFN(dl_sf, sf_symbols);
//  }
//}

//void PDCCH_ENODEB::encodeSubframeDataNormal(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[]) {
//  srslte_dci_msg_t dci_msg;
//  // PDCCH + PDSCH
//  dl_sf.sf_type = SRSLTE_SF_NORM;

//  /* Encode PDCCH */
//  srslte_dci_location_t location = searchSpace[dl_sf.cfi - 1][dl_sf.tti % 10]->getLocations()[0];
//  INFO("Putting DCI to location: n=%d, L=%d\n", location.ncce, location.L);

//  srslte_dci_msg_pack_pdsch(&cell, &dl_sf, nullptr, &dci_dl, &dci_msg);
//  dci_msg.location = location;
//  if (srslte_pdcch_encode(&pdcch, &dl_sf, &dci_msg, sf_symbols)) {
//    ERROR("Error encoding DCI message\n");
//    exit(-1);
//  }

//  /* Configure pdsch_cfg parameters */
//  if (srslte_ra_dl_dci_to_grant(&cell, &dl_sf, transmission_mode, enable_256qam, &dci_dl, &pdsch_cfg.grant)) {
//    ERROR("Error configuring PDSCH\n");
//    exit(-1);
//  }

//  /* Encode PDSCH */
//  if (srslte_pdsch_encode(&pdsch, &dl_sf, &pdsch_cfg, data, sf_symbols)) {
//    ERROR("Error encoding PDSCH\n");
//    exit(-1);
//  }
//  if (net_port > 0 && net_packet_ready) {
//    if (null_file_sink) {
//      for (uint32_t tb = 0; tb < SRSLTE_MAX_CODEWORDS; tb++) {
//        srslte_bit_pack_vector(data[tb], data_tmp, pdsch_cfg.grant.tb[tb].tbs);
//        if (srslte_netsink_write(&net_sink, data_tmp, 1 + (pdsch_cfg.grant.tb[tb].tbs - 1) / 8) < 0) {
//          ERROR("Error sending data through UDP socket\n");
//        }
//      }
//    }
//    if(mbsfn_area_id < 0){
//      net_packet_ready = false;
//      sem_post(&net_sem);
//    }
//  }
//}

//void PDCCH_ENODEB::encodeSubframeDataMBSFN(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[SRSLTE_MAX_PORTS]) {
//  // We're sending MCH on subframe 1 - PDCCH + PMCH
//  dl_sf.sf_type = SRSLTE_SF_MBSFN;

//  /* Force 1 word and MCS 2 */
//  dci_dl.rnti                    = SRSLTE_MRNTI;
//  dci_dl.alloc_type              = SRSLTE_RA_ALLOC_TYPE0;
//  dci_dl.type0_alloc.rbg_bitmask = 0xffffffff;
//  dci_dl.tb[0].mcs_idx           = 2;
//  dci_dl.format                  = SRSLTE_DCI_FORMAT1;

//  /* Configure pdsch_cfg parameters */
//  if (srslte_ra_dl_dci_to_grant(&cell, &dl_sf, SRSLTE_TM1, enable_256qam, &dci_dl, &pmch_cfg.pdsch_cfg.grant)) {
//    ERROR("Error configuring PDSCH\n");
//    exit(-1);
//  }

//  for (int i = 0; i < pmch_cfg.pdsch_cfg.grant.tb[0].tbs / 8; i++) {
//    data_mbms[i] = i % 255;
//  }

//  pmch_cfg.area_id = (uint16_t)mbsfn_area_id;

//  /* Encode PMCH */
//  if (srslte_pmch_encode(&pmch, &dl_sf, &pmch_cfg, data_mbms, sf_symbols)) {
//    ERROR("Error encoding PDSCH\n");
//    exit(-1);
//  }
//  if (net_port > 0 && net_packet_ready) {
//    if (null_file_sink) {
//      srslte_bit_pack_vector(data[0], data_tmp, pmch_cfg.pdsch_cfg.grant.tb[0].tbs);
//      if (srslte_netsink_write(&net_sink, data_tmp, 1 + (pmch_cfg.pdsch_cfg.grant.tb[0].tbs - 1) / 8) < 0) {
//        ERROR("Error sending data through UDP socket\n");
//      }
//    }
//    net_packet_ready = false;
//    sem_post(&net_sem);
//  }
//}

void PDCCH_ENODEB::fftSamples(bool mbsfn) {
  /* Transform to OFDM symbols */
  if(!mbsfn) {
    for (uint32_t i = 0; i < cell.nof_ports; i++) {
      srslte_ofdm_tx_sf(&ifft[i]);
    }
  }
  else {
    srslte_ofdm_tx_sf(&ifft_mbsfn);
  }
}

void PDCCH_ENODEB::sendSamples(bool start_of_burst) {
  /* send to file or usrp */
  if (output_file_name) {
    if (!null_file_sink) {
      /* Apply AWGN */
      if (output_file_snr != +INFINITY) {
        float var = powf(10.0f, -(output_file_snr + 3.0f) / 20.0f);
        for (uint32_t k = 0; k < cell.nof_ports; k++) {
          srslte_ch_awgn_c(output_buffer[k], output_buffer[k], var, (uint32_t)sf_n_samples);
        }
      }
      srslte_filesink_write_multi(&fsink, (void**) output_buffer, sf_n_samples, (int)cell.nof_ports);
    }
    usleep(1000);
  }
  else {
#ifndef DISABLE_RF
    float norm_factor = (float) cell.nof_prb/15/sqrtf(pdsch_cfg.grant.nof_prb);
    for (uint32_t i = 0; i < cell.nof_ports; i++) {
      srslte_vec_sc_prod_cfc(output_buffer[i], rf_amp * norm_factor, output_buffer[i], (uint32_t)(SRSLTE_SF_LEN_PRB(cell.nof_prb)));
    }
    srslte_rf_send_multi(&rf, (void**) output_buffer, sf_n_samples, true, start_of_burst, false);
    start_of_burst=false;
#endif
  }
}
