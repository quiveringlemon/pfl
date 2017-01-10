/*******************************************************************\

Module: Base class for command line interpretation

Author: CM Wintersteiger

Date: June 2006

\*******************************************************************/

#ifndef CPROVER_GOTO_CC_CW_MODE_H
#define CPROVER_GOTO_CC_CW_MODE_H

#include "goto_cc_mode.h"
#include "gcc_cmdline.h"

class cw_modet:public goto_cc_modet
{
public:
  virtual int doit();
  virtual void help_mode();

  explicit cw_modet(gcc_cmdlinet &_gcc_cmdline):
    goto_cc_modet(_gcc_cmdline),
    cmdline(_gcc_cmdline)
  {
  }

protected:
  gcc_cmdlinet &cmdline;
};

#endif // CPROVER_GOTO_CC_CW_MODE_H
