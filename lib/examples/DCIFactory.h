#pragma once

#include <vector>

#include "srslte/srslte.h"

#include "DCIFileReplay.h"

class DCIFactory {
public:
  DCIFactory();
  virtual ~DCIFactory();

  virtual bool ended() = 0;
  virtual bool nextTTI() = 0;
  virtual uint32_t getTTI() const = 0;
  virtual std::vector<srslte_dci_dl_t> getDCI_DL() const = 0;
  virtual std::vector<srslte_dci_ul_t> getDCI_UL() const = 0;
  virtual srslte_cell_t& getCell() = 0;
  virtual uint32_t getCFI() const = 0;
private:

};

class DCIFileReplay;  //forward declaration

class DCIFromFile : public DCIFactory {
public:
  DCIFromFile(DCIFileReplay& fileReplay, srslte_cell_t& cell);
  virtual ~DCIFromFile() override;

  virtual bool ended() override;
  virtual bool nextTTI() override;
  virtual uint32_t getTTI() const override;
  virtual std::vector<srslte_dci_dl_t> getDCI_DL() const override;
  virtual std::vector<srslte_dci_ul_t> getDCI_UL() const override;
  virtual srslte_cell_t& getCell() override;
  virtual uint32_t getCFI() const override;
private:
  DCIFileReplay& fileReplay;
  srslte_cell_t& cell;
  std::vector<srslte_dci_dl_t> dci_dl;
  std::vector<srslte_dci_ul_t>  dci_ul;
  uint32_t tti;
  uint32_t cfi;
};

class DCILine {
public:
  DCILine(std::string line, DCIFromFile& parent);
  bool isDownlink();
  uint32_t getTTI();
  uint32_t getCFI();
  srslte_dci_dl_t toDCI_DL();
  srslte_dci_ul_t toDCI_UL();
private:
  DCIFromFile& parent;

  double timestamp;
  uint32_t sfn;
  uint32_t sf_idx;
  uint16_t msg_rnti;
  uint32_t direction;
  uint32_t mcs_idx;
  uint32_t L_prb;
  uint32_t mcs_tbs_sum;
  int32_t mcs0_tbs;
  int32_t mcs1_tbs;
  uint32_t format;
  uint32_t ndi;
  int32_t ndi_1;
  uint32_t harq_process;
  uint32_t ncce;
  uint32_t aggregation;
  uint32_t cfi;
  uint32_t histval;
  uint32_t msg_len;
  uint8_t  payload[SRSLTE_DCI_MAX_BITS];
};
