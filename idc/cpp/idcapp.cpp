#include "idcapp.h"

#include "_public.h"

bool CZHOBTMIND::splitbuffer(const string& strbuffer, const bool bisxml) {
  memset(&m_stzhobtmind, 0, sizeof(struct st_zhobtmind));

  if (bisxml == true) {
    // 解析xml行,放在结构体中
    getxmlbuffer(strbuffer, "obtid", m_stzhobtmind.obtid, 5);
    getxmlbuffer(strbuffer, "ddatetime", m_stzhobtmind.ddatetime, 14);
    getxmlbuffer(strbuffer, "t", m_stzhobtmind.t, 10);
    getxmlbuffer(strbuffer, "p", m_stzhobtmind.p, 10);
    getxmlbuffer(strbuffer, "u", m_stzhobtmind.u, 10);
    getxmlbuffer(strbuffer, "wd", m_stzhobtmind.wd, 10);
    getxmlbuffer(strbuffer, "wf", m_stzhobtmind.wf, 10);
    getxmlbuffer(strbuffer, "r", m_stzhobtmind.r, 10);
  } else {
    ccmdstr cmdstr;
    cmdstr.splittocmd(strbuffer, ",");
    cmdstr.getvalue(0, m_stzhobtmind.obtid, 5);
    cmdstr.getvalue(1, m_stzhobtmind.ddatetime, 14);
    cmdstr.getvalue(2, m_stzhobtmind.t, 10);
    cmdstr.getvalue(3, m_stzhobtmind.p, 10);
    cmdstr.getvalue(4, m_stzhobtmind.u, 10);
    cmdstr.getvalue(5, m_stzhobtmind.wd, 10);
    cmdstr.getvalue(6, m_stzhobtmind.wf, 10);
    cmdstr.getvalue(7, m_stzhobtmind.r, 10);
    cmdstr.getvalue(8, m_stzhobtmind.vis, 10);
  }
  m_buffer = strbuffer;

  return true;
}


bool CZHOBTMIND::inserttable() {
  // 如有文件需要处理，先判断语句对象是否已打开，再准备sql语句
  if (m_stmt.isopen() == false) {
    m_stmt.connect(&m_conn);
    m_stmt.prepare(
      "insert into T_ZHOBTMIND(obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid) "
      "values(:1,to_date(:2,'yyyymmddhh24miss'),:3*10,:4*10,:5,:6,:7*10,:8*10,:9*10,SEQ_"
      "ZHOBTMIND.NEXTVAL)"
    );
    m_stmt.bindin(1, m_stzhobtmind.obtid, 5);
    m_stmt.bindin(2, m_stzhobtmind.ddatetime, 14);
    m_stmt.bindin(3, m_stzhobtmind.t, 10);
    m_stmt.bindin(4, m_stzhobtmind.p, 10);
    m_stmt.bindin(5, m_stzhobtmind.u, 10);
    m_stmt.bindin(6, m_stzhobtmind.wd, 10);
    m_stmt.bindin(7, m_stzhobtmind.wf, 10);
    m_stmt.bindin(8, m_stzhobtmind.r, 10);
    m_stmt.bindin(9, m_stzhobtmind.vis, 10);
  }
  // 插入数据库, 插入失败可能是记录重复或数据内容非法
  if (m_stmt.execute() != 0) {
    if (m_stmt.rc() != 1) {
      m_logfile.write("strbuffer=%s\n", m_buffer.c_str());
      m_logfile.write("m_stmt.execute() failed.\n%s\n%s\n", m_stmt.sql(), m_stmt.message());
    }
    return false;
  }
  return true;
}
