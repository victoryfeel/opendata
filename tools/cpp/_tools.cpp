#include "_tools.h"

TableColumn::TableColumn() {
  init_data();
}

void TableColumn::init_data() {
  m_column_info.clear();
  m_primary_key_info.clear();
  m_all_columns.clear();
  m_all_primary_key.clear();
}


bool TableColumn::get_column_info(connection& conn, char* table_name) {
  m_column_info.clear();
  m_all_columns.clear();

  struct st_column stcolumn;
  sqlstatement stmt(&conn);
  // 从USER_TAB_COLUMNS字典中获取表全部的字段，注意：1）把结果集转换成小写；2）数据字典中的表名是大写。
  stmt.prepare(
    "select lower(column_name), lower(data_type), data_length from USER_TAB_COLUMNS where "
    "table_name = upper(:1) order by column_id"
  );
  stmt.bindin(1, table_name, 30);
  stmt.bindout(1, stcolumn.column_name, 30);
  stmt.bindout(2, stcolumn.data_type, 30);
  stmt.bindout(3, stcolumn.column_length);
  if (stmt.execute() != 0) {
    return false;
  }

  while (true) {
    memset(&stcolumn, 0, sizeof(struct st_column));
    if (stmt.next() != 0) {
      break;
    }

    // cpp操作oracle数据库，datatype转为char,date,number三类
    // 如果业务有需要，可以修改以下的代码，增加对更多数据类型的支持。

    // 所有字符串型都转为char, rowid当成字符串处理，长度为18byte
    if (strcmp(stcolumn.data_type, "char") == 0) {
      strcpy(stcolumn.data_type, "char");
    }
    if (strcmp(stcolumn.data_type, "nchar") == 0) {
      strcpy(stcolumn.data_type, "char");
    }
    if (strcmp(stcolumn.data_type, "varchar2") == 0) {
      strcpy(stcolumn.data_type, "char");
    }
    if (strcmp(stcolumn.data_type, "nvarchar2") == 0) {
      strcpy(stcolumn.data_type, "char");
    }
    if (strcmp(stcolumn.data_type, "rowid") == 0) {
      strcpy(stcolumn.data_type, "char");
      stcolumn.column_length = 18;
    }
    // 日期转为date "yyyymmddhh24miss"
    if (strcmp(stcolumn.data_type, "date") == 0) {
      stcolumn.column_length = 14;
    }
    // 数字类型转为number
    if (strcmp(stcolumn.data_type, "number") == 0) {
      strcpy(stcolumn.data_type, "number");
    }
    if (strcmp(stcolumn.data_type, "integer") == 0) {
      strcpy(stcolumn.data_type, "number");
    }
    if (strcmp(stcolumn.data_type, "float") == 0) {
      strcpy(stcolumn.data_type, "number");
    }
    // ignore other types
    if (
      (strcmp(stcolumn.data_type, "char") != 0) && (strcmp(stcolumn.data_type, "date") != 0) &&
      (strcmp(stcolumn.data_type, "number") != 0)
    ) {
      continue;
    }
    // set number type length to 22
    if (strcmp(stcolumn.data_type, "number") == 0) {
      stcolumn.column_length = 22;
    }

    // 拼接所有字段名，逗号分隔,最后会多出一个逗号 "a,b,c,"
    m_all_columns = m_all_columns + stcolumn.column_name + ",";
    m_column_info.push_back(stcolumn);
  }

  // 删除m_all_columns最后一个逗号
  if (stmt.rpc() > 0) {
    deleterchr(m_all_columns, ',');
  }

  return true;
}


bool TableColumn::get_primary_key_info(connection& conn, char* table_name) {
  m_primary_key_info.clear();
  m_all_primary_key.clear();

  struct st_column stcolumn;
  sqlstatement stmt(&conn);
  stmt.prepare(
    "select lower(column_name), position from USER_CONS_COLUMNS where table_name = upper(:1) and "
    "constraint_name =(select constraint_name from USER_CONSTRAINTS where "
    "table_name = upper(:2) and constraint_type = 'P' and generated = 'USER NAME') order by "
    "position"
  );

  stmt.bindin(1, table_name, 30);
  stmt.bindin(2, table_name, 30);
  stmt.bindout(1, stcolumn.column_name, 30);
  stmt.bindout(2, stcolumn.primary_key_sequence);
  if (stmt.execute() != 0) {
    return false;
  }

  while (true) {
    memset(&stcolumn, 0, sizeof(struct st_column));
    if (stmt.next() != 0) {
      break;
    }

    m_all_primary_key = m_all_primary_key + stcolumn.column_name + ",";
    m_primary_key_info.push_back(stcolumn);
  }

  if (stmt.rpc() > 0) {
    deleterchr(m_all_primary_key, ',');
  }

  // 更新m_column_info中的primary_key_sequence字段，
  // 0表示非主键，1表示第一个主键，2表示第二个主键，以此类推
  for (auto& i : m_primary_key_info) {
    for (auto& j : m_column_info) {
      if (strcmp(i.column_name, j.column_name) == 0) {
        j.primary_key_sequence = i.primary_key_sequence;
        break;
      }
    }
  }

  return true;
}
