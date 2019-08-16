
#pragma once

#include <vector>

#include <pthread.h>
#include <semaphore.h>

#include "DCIFileReplay.h"
#include "DCIFactory.h"

#include "srslte/common/crash_handler.h"
#include "srslte/common/gen_mch_tables.h"
#include "srslte/srslte.h"

// include C-only headers
#ifdef __cplusplus
    extern "C" {
#endif
#ifndef DISABLE_RF
#include "srslte/phy/rf/rf.h"
#include "srslte/phy/common/phy_common.h"
#else
#pragma message "Compiling pdsch_ue with no RF support"
#endif
#ifdef __cplusplus
}
#undef I // Fix complex.h #define I nastiness when using C++
#endif

class SearchSpace {
public:
  SearchSpace(uint16_t rnti, uint32_t sf_idx, uint32_t ncce);
  ~SearchSpace();
  uint16_t getRNTI();
  std::vector<srslte_dci_location_t>& getLocations();

private:
  uint16_t rnti;
  std::vector<srslte_dci_location_t> locations;

};

class PDCCH_ENODEB {
public:
    PDCCH_ENODEB();
    ~PDCCH_ENODEB();
    void usage(char* prog);
    void parse_args(int argc, char **argv);
    void base_init();
    void base_free();
    static void* net_thread_fnc(void *arg);
    void* _net_thread_fnc();
    int run(int argc, char **argv);

private:
    unsigned int reverse(unsigned int x);
    uint32_t prbset_to_bitmask();
    int update_radl();
    int update_control();

    //split methods
    void updateSearchSpace();
    void putDCIfromFactory(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[SRSLTE_MAX_PORTS], DCIFactory& factory);
    void encodeSubframeData(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[SRSLTE_MAX_PORTS], bool mbsfn);
    void encodeSubframeDataNormal(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[SRSLTE_MAX_PORTS]);
    void encodeSubframeDataMBSFN(srslte_dl_sf_cfg_t& dl_sf, cf_t* sf_symbols[SRSLTE_MAX_PORTS]);
    void fftSamples(bool mbsfn);
    void sendSamples(bool start_of_burst);

    char *input_file_name = nullptr;
    char *output_file_name = nullptr;

#define LEFT_KEY  68
#define RIGHT_KEY 67
#define UP_KEY    65
#define DOWN_KEY  66

    srslte_cell_t cell = {
        25,                // nof_prb
        1,                 // nof_ports
        0,                 // cell_id
        SRSLTE_CP_NORM,    // cyclic prefix
        SRSLTE_PHICH_NORM, // PHICH length
        SRSLTE_PHICH_R_1,  // PHICH resources
        SRSLTE_FDD,

    };

#define UE_CRNTI 0x1234
#define M_CRNTI 0xFFFD

#ifndef DISABLE_RF
    srslte_rf_t rf;
#endif

    //int net_port = -1; // -1 generates random dataThat means there is some problem sending samples to the device

    uint32_t cfi = 2;
    uint32_t mcs_idx = 1, last_mcs_idx = 1;
    int nof_frames = -1;

    srslte_tm_t transmission_mode    = SRSLTE_TM1;
    uint32_t nof_tb = 1;
    uint32_t multiplex_pmi = 0;
    uint32_t multiplex_nof_layers = 1;
    uint8_t  mbsfn_sf_mask = 32;
    int mbsfn_area_id = -1;
    char *rf_args = "";
    char *rf_dev = "";
    float rf_amp = 0.8f, rf_gain = 60.0, rf_freq = 2400000000;
    bool enable_256qam = false;

    float output_file_snr = +INFINITY;

    bool null_file_sink=false;
    srslte_filesink_t fsink;
    srslte_ofdm_t ifft[SRSLTE_MAX_PORTS];
    srslte_ofdm_t ifft_mbsfn;
    srslte_pbch_t pbch;
    srslte_pcfich_t pcfich;
    srslte_pdcch_t pdcch;
    srslte_pdsch_t pdsch;
    srslte_pdsch_cfg_t pdsch_cfg;
    srslte_pmch_t pmch;
    srslte_pmch_cfg_t       pmch_cfg;
    srslte_softbuffer_tx_t *softbuffers[SRSLTE_MAX_CODEWORDS];
    srslte_regs_t regs;
    srslte_dci_dl_t         dci_dl;
    int rvidx[SRSLTE_MAX_CODEWORDS] = {0, 0};

    cf_t *sf_buffer[SRSLTE_MAX_PORTS] = {nullptr}, *output_buffer [SRSLTE_MAX_PORTS] = {nullptr};

    int sf_n_re, sf_n_samples;

    //pthread_t net_thread;
    //void *net_thread_fnc(void *arg);
    //sem_t net_sem;
    //bool net_packet_ready = false;
    //srslte_netsource_t net_source;
    //srslte_netsink_t net_sink;


    int prbset_num = 1, last_prbset_num = 1;
    int prbset_orig = 0;
    //#define DATA_BUFF_SZ    1024*128
    //uint8_t data[8*DATA_BUFF_SZ], data2[DATA_BUFF_SZ];
    //uint8_t data_tmp[DATA_BUFF_SZ];


#define DATA_BUFF_SZ    1024*1024
    uint8_t *data_mbms, *data[2], data2[DATA_BUFF_SZ];
    uint8_t data_tmp[DATA_BUFF_SZ];

    SearchSpace* searchSpace[3][SRSLTE_NOF_SF_X_FRAME];
    DCIFileReplay fileReplay;
};
