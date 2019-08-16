#include "DCIFactory.h"
#include "CSV.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "srslte/phy/phch/dci.h"
#include "srslte/phy/common/phy_common.h"

#ifdef __cplusplus
}
#endif

#include <sstream>
#include <strings.h>


void sprint_hex(char *str, const uint32_t max_str_len, uint8_t *x, const uint32_t len) {
  uint32_t i, nbytes;
  uint8_t byte;
  nbytes = len/8;
  // check that hex string fits in buffer (every byte takes 2 characters)
  if ((2*(len/8 + ((len%8)?1:0))) >= max_str_len) {
    ERROR("Buffer too small for printing hex string (max_str_len=%d, payload_len=%d).\n", max_str_len, len);
    return;
  }

  int n=0;
  for (i=0;i<nbytes;i++) {
    byte = (uint8_t) srslte_bit_pack(&x, 8);
    n+=sprintf(&str[n], "%02x", byte);
  }
  if (len%8) {
    byte = (uint8_t) srslte_bit_pack(&x, len%8)<<(8-(len%8));
    n+=sprintf(&str[n], "%02x", byte);
  }
  str[n] = 0;
  str[max_str_len-1] = 0;
}

void sscan_hex(const char *str, uint8_t *x, const uint32_t len) {
  size_t str_len = strlen(str);
  uint8_t byte;

  if( (2*(len/8 + (len%8?1:0))) > str_len ) {
    ERROR("Hex string too short (%ld byte) to fill bit string of length %d", str_len, len);
    return;
  }

  int pos=0;
  for(uint32_t n=0; n<len; n+=8) {
    sscanf(&str[pos], "%02hhx", &byte); // read two chars (as hex)
    pos += 2;
    srslte_bit_unpack(byte, &x, 8);
  }
  if(len%8) {
    sscanf(&str[pos], "%02hhx", &byte);
    srslte_bit_unpack(byte<<(8-(len%8)), &x, len%8);
  }

}


DCIFactory::DCIFactory() {

}

DCIFactory::~DCIFactory() {

}


DCIFromFile::DCIFromFile(DCIFileReplay& fileReplay, srslte_cell_t& cell) :
  fileReplay(fileReplay),
  cell(cell),
  tti(0)
{

}

DCIFromFile::~DCIFromFile() {

}

bool DCIFromFile::ended() {
  return !fileReplay.hasNew();
}

bool DCIFromFile::nextTTI() {
  dci_dl.clear();
  dci_ul.clear();

  if(fileReplay.hasNew()) {
    DCILine dciLine(fileReplay.getLine(), *this);
    tti = dciLine.getTTI();
    cfi = dciLine.getCFI();
    do {
      DCILine dciLine(fileReplay.getLine(), *this);
      if(dciLine.getTTI() != tti) {
        break;
      }
      if(dciLine.isDownlink()) {
        dci_dl.push_back(dciLine.toDCI_DL());
      }
      else {
        dci_ul.push_back(dciLine.toDCI_UL());
      }
    } while(fileReplay.nextLine());

    return true;
  }
  return false;
}

uint32_t DCIFromFile::getTTI() const {
  return tti;
}

uint32_t DCIFromFile::getCFI() const {
  return cfi;
}

std::vector<srslte_dci_dl_t> DCIFromFile::getDCI_DL() const {
  return dci_dl;
}

std::vector<srslte_dci_ul_t> DCIFromFile::getDCI_UL() const {
  return dci_ul;
}

srslte_cell_t& DCIFromFile::getCell() {
  return cell;
}


DCILine::DCILine(std::string line, DCIFromFile& parent) :
  parent(parent)
{
  std::vector<std::string> tokens;
  CSV::splitString(line, '\t', tokens, 20);
  std::vector<std::string>::iterator token = tokens.begin();
  std::stringstream(*token++) >> timestamp;
  std::stringstream(*token++) >> sfn;
  std::stringstream(*token++) >> sf_idx;
  std::stringstream(*token++) >> msg_rnti;
  std::stringstream(*token++) >> direction;
  std::stringstream(*token++) >> mcs_idx;
  std::stringstream(*token++) >> L_prb;
  std::stringstream(*token++) >> mcs_tbs_sum;
  std::stringstream(*token++) >> mcs0_tbs;
  std::stringstream(*token++) >> mcs1_tbs;
  std::stringstream(*token++) >> format;
  std::stringstream(*token++) >> ndi;
  std::stringstream(*token++) >> ndi_1;
  std::stringstream(*token++) >> harq_process;
  std::stringstream(*token++) >> ncce;
  std::stringstream(*token++) >> aggregation;
  std::stringstream(*token++) >> cfi;
  std::stringstream(*token++) >> histval;
  std::stringstream(*token++) >> msg_len;
  std::string tmp;
  std::stringstream(*token++) >> tmp;
  sscan_hex(tmp.c_str(), payload, msg_len);
}

bool DCILine::isDownlink() {
  return direction == 1;
}

uint32_t DCILine::getTTI() {
  return (sfn % 1024) * 10 + sf_idx;
}

uint32_t DCILine::getCFI() {
  return cfi;
}

srslte_dci_ul_t DCILine::toDCI_UL() {
  srslte_dci_ul_t result;

  // configure the packed dci msg
  srslte_dci_msg_t msg;
  memcpy(msg.payload, payload, msg_len);
  msg.nof_bits = msg_len;
  msg.location.L = aggregation;
  msg.location.ncce = ncce;
  msg.format = static_cast<srslte_dci_format_t>(format);
  msg.rnti = msg_rnti;

  // these objects are currently not required, pass nullptr
  //srslte_dl_sf_cfg_t dl_sf;
  //srslte_dci_cfg_t cfg;

  srslte_dci_msg_unpack_pusch(&parent.getCell(), nullptr, nullptr, &msg, &result);

  // cross-check provided info with unpacked dci contents
  bool ok = true;
  if(result.rnti != msg_rnti) {
    ERROR("Inconsistent RNTI");
    ok = false;
  }
  if(result.format != static_cast<srslte_dci_format_t>(format)) {
    ERROR("Inconsistent format");
    ok = false;
  }
  if(result.location.L != aggregation) {
    ERROR("Inconsistent aggregation level");
    ok = false;
  }
  if(result.location.ncce != ncce) {
    ERROR("Inconsistent ncce in DCI location");
    ok = false;
  }
  if(result.tb.mcs_idx != mcs_idx) {
    ERROR("Inconsistent MCS");
    ok = false;
  }
  if(result.tb.ndi != ndi) {
    ERROR("Inconsistent NDI");
    ok = false;
  }

  if(!ok) {
    ERROR("Produced inconsistent DCI");
  }

  return result;
}

srslte_dci_dl_t DCILine::toDCI_DL() {
  srslte_dci_dl_t result;

  // configure the packed dci msg
  srslte_dci_msg_t msg;
  memcpy(msg.payload, payload, msg_len);
  msg.nof_bits = msg_len;
  msg.location.L = aggregation;
  msg.location.ncce = ncce;
  msg.format = static_cast<srslte_dci_format_t>(format-1);  //index of DL formats is printed as format+1
  msg.rnti = msg_rnti;

  // these objects are currently not required, pass nullptr
  //srslte_dl_sf_cfg_t dl_sf;
  //srslte_dci_cfg_t cfg;

  if(msg.rnti == 56738) {
    printf("HALT!");
  }

  srslte_dci_msg_unpack_pdsch(&parent.getCell(), nullptr, nullptr, &msg, &result);

  // cross-check provided info with unpacked dci contents
  bool ok = true;
  if(result.rnti != msg_rnti) {
    ERROR("Inconsistent RNTI");
    ok = false;
  }
  if(result.format != static_cast<srslte_dci_format_t>(format-1)) {
    ERROR("Inconsistent format");
    ok = false;
  }
  if(result.location.L != aggregation) {
    ERROR("Inconsistent aggregation level");
    ok = false;
  }
  if(result.location.ncce != ncce) {
    ERROR("Inconsistent ncce in DCI location");
    ok = false;
  }
  if(result.tb[0].mcs_idx != mcs_idx) {
    ERROR("Inconsistent MCS");
    ok = false;
  }
  if(result.tb[0].ndi != ndi) {
    ERROR("Inconsistent NDI");
    ok = false;
  }

  switch(result.format) {
    case SRSLTE_DCI_FORMAT2:
    case SRSLTE_DCI_FORMAT2A:
    case SRSLTE_DCI_FORMAT2B:
      if(result.tb[1].ndi != ndi_1 && result.format == SRSLTE_DCI_FORMAT1C) {
        ERROR("Inconsistent NDI_1");
        ok = false;
      }
      break;
    default:
      break;
  }

  if(!ok) {
    ERROR("Produced inconsistent DCI");
  }

  return result;
}
