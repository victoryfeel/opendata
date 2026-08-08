#ifndef _TOOLS_H
#define _TOOLS_H

#include "_ooci.h"
#include "_public.h"
using namespace idc;

class TableColumn {
private:
  struct st_column {
    char column_name[31];
    char data_type[31];
    int column_length;
    int primary_key_sequence;
  };

public:
  TableColumn();

  vector<struct st_column> m_column_info;
  vector<struct st_column> m_primary_key_info;
  string m_all_columns;
  string m_all_primary_key;

  void init_data();
  bool get_column_info(connection& conn, char* table_name);
  bool get_primary_key_info(connection& conn, char* table_name);
};


#endif
