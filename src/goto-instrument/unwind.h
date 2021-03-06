/*******************************************************************\

Module: loop unwinding

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#ifndef CPROVER_UNWIND_H
#define CPROVER_UNWIND_H

#include <goto-programs/goto_program.h>

void unwind(
  goto_programt &goto_program,
  goto_programt::targett loop_head,
  goto_programt::targett loop_exit,
  const unsigned k);

void unwind(
  goto_programt &goto_program,
  goto_programt::targett loop_head,
  goto_programt::targett loop_exit,
  const unsigned k,
  std::vector<goto_programt::targett> &iteration_points);

void goto_unwind(
  goto_functionst &goto_functions,
  const unsigned k);

void goto_unwind_type2(
  goto_functionst &goto_functions,
  const unsigned k);

#endif
