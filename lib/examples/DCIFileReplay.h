#pragma once

#include <string>
#include <fstream>

#include "DCIFactory.h"

class DCIFactory;   //forward declaration
class DCIFromFile;

class DCIFileReplay {
public:
  DCIFileReplay(srslte_cell_t& cell);
  ~DCIFileReplay();
  bool openFile(std::string filename);
  srslte_cell_t& getCell();
  bool hasNew() const;
  bool nextLine();
  std::string getLine() const;
  DCIFactory& getFactory();
private:
  srslte_cell_t& cell;
  DCIFromFile* dciFactory;
  std::ifstream infile;
  std::string line;
};
