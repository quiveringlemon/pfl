/*******************************************************************

 Module: Counterexample-Guided Inductive Synthesis

 Author: Daniel Kroening, kroening@kroening.com
         Pascal Kesseli, pascal.kesseil@cs.ox.ac.uk

\*******************************************************************/

#ifndef CEGIS_ENCODED_SAFETY_LEARN_CONFIG_H_
#define CEGIS_ENCODED_SAFETY_LEARN_CONFIG_H_

#include <cegis/value/program_individual.h>

#include <cegis/safety/symex/learn/safety_learn_config.h>

/**
 * @brief
 *
 * @details
 */
class encoded_safety_learn_configt
{
  class safety_learn_configt &config;
public:
  typedef safety_learn_configt::counterexamplet counterexamplet;
  typedef safety_learn_configt::counterexamplest counterexamplest;
  typedef program_individualt candidatet;

  /**
   * @brief
   *
   * @details
   *
   * @param config
   */
  encoded_safety_learn_configt(safety_learn_configt &config);

  /**
   * @brief
   *
   * @details
   */
  ~encoded_safety_learn_configt();

  /**
   * @brief
   *
   * @details
   *
   * @param counterexamples
   * @param max_solution_size
   */
  void process(const counterexamplest &counterexamples,
      size_t max_solution_size);

  /**
   * @brief
   *
   * @details
   *
   * @param word_width_in_bits
   */
  void set_word_width(size_t word_width_in_bits);

  /**
   * @brief
   *
   * @details
   *
   * @return
   */
  const symbol_tablet &get_symbol_table() const;

  /**
   * @brief
   *
   * @details
   *
   * @return
   */
  const goto_functionst &get_goto_functions() const;

  /**
   * @brief
   *
   * @details
   *
   * @param current_candidate
   * @param trace
   * @param max_solution_size
   */
  void convert(candidatet &current_candidate, const class goto_tracet &trace,
      size_t max_solution_size);

  /**
   * @brief
   *
   * @details
   *
   * @param os
   * @param candidate
   */
  void show_candidate(messaget::mstreamt &os, const candidatet &candidate);

  /**
   * @brief
   *
   * @details
   *
   * @return
   */
  size_t get_num_vars() const;

  /**
   * @brief
   *
   * @details
   *
   * @return
   */
  size_t get_num_consts() const;
};

#endif /* CEGIS_ENCODED_SAFETY_LEARN_CONFIG_H_ */
