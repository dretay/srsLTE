#include "DCIFileReplay.h"

DCIFileReplay::DCIFileReplay(srslte_cell_t& cell) :
  cell(cell)
{
  dciFactory = new DCIFromFile(*this, cell);
}

DCIFileReplay::~DCIFileReplay() {
  delete dciFactory;
}

bool DCIFileReplay::openFile(std::string filename) {

  // open file and save the first line
  infile = std::ifstream(filename);
  std::getline(infile, line);
  if(line.length() == 0) {
    return false;
  }

  // cell settings could be read from some file, too
  // currently, don't change the provided cell object
  /*
  cell.nof_prb = 50;
  cell.nof_ports = 1;
  cell.id = 0;
  cell.cp = SRSLTE_CP_NORM;
  cell.phich_length = SRSLTE_PHICH_NORM;
  cell.phich_resources = SRSLTE_PHICH_R_1;
  cell.frame_type = SRSLTE_FDD;
  */

  return true;
}

srslte_cell_t& DCIFileReplay::getCell() {
  return cell;
}

bool DCIFileReplay::hasNew() const {
  return line.length() > 0;
}

std::string DCIFileReplay::getLine() const {
  return line;
}

bool DCIFileReplay::nextLine() {
  std::getline(infile, line);
  return hasNew();
}

DCIFactory& DCIFileReplay::getFactory() {
  return *dciFactory;
}
