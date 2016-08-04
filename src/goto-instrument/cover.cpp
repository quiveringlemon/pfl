/*******************************************************************\

Module: Coverage Instrumentation

Author: Daniel Kroening

Date: May 2016

\*******************************************************************/

#include <iostream>
#include <algorithm>
#include <iterator>

#include <util/prefix.h>
#include <util/i2string.h>
#include <util/expr_util.h>

#include "cover.h"

class basic_blockst
{
public:
  explicit basic_blockst(const goto_programt &_goto_program)
  {
    bool next_is_target=true;
    unsigned block_count=0;

    forall_goto_program_instructions(it, _goto_program)
    {
      if(next_is_target || it->is_target())
        block_count++;
        
      block_map[it]=block_count;
      
      if(!it->source_location.is_nil() &&
         source_location_map.find(block_count)==source_location_map.end())
        source_location_map[block_count]=it->source_location;
      
      next_is_target=
        it->is_goto() || it->is_function_call();
    }
  }

  // map program locations to block numbers  
  typedef std::map<goto_programt::const_targett, unsigned> block_mapt;
  block_mapt block_map;
  
  // map block numbers to source code locations
  typedef std::map<unsigned, source_locationt> source_location_mapt;
  source_location_mapt source_location_map;
  
  inline unsigned operator[](goto_programt::const_targett t)
  {
    return block_map[t];
  }
  
  void output(std::ostream &out)
  {
    for(block_mapt::const_iterator
        b_it=block_map.begin();
        b_it!=block_map.end();
        b_it++)
      out << b_it->first->source_location
          << " -> " << b_it->second
          << '\n';
  }
};

/*******************************************************************\

Function: as_string

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

const char *as_string(coverage_criteriont c)
{
  switch(c)
  {
  case coverage_criteriont::LOCATION: return "location";
  case coverage_criteriont::BRANCH: return "branch";
  case coverage_criteriont::DECISION: return "decision";
  case coverage_criteriont::CONDITION: return "condition";
  case coverage_criteriont::PATH: return "path";
  case coverage_criteriont::MCDC: return "MC/DC";
  case coverage_criteriont::ASSERTION: return "assertion";
  case coverage_criteriont::COVER: return "cover instructions";
  default: return "";
  }
}

/*******************************************************************\

Function: is_condition

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool is_condition(const exprt &src)
{
  if(src.type().id()!=ID_bool) return false;

  // conditions are 'atomic predicates'
  if(src.id()==ID_and || src.id()==ID_or ||
     src.id()==ID_not || src.id()==ID_implies)
    return false;
  
  return true;
}

bool all_conditions(const std::vector<exprt> &operands)
{
  for(const auto &p : operands)
    if(not is_condition(p))
      return false;
  return true;
}







/*******************************************************************\

Function: collect_conditions_rec

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void collect_conditions_rec(const exprt &src, std::set<exprt> &dest)
{
  if(src.id()==ID_address_of)
  {
    return;
  }

  for(const auto & op : src.operands())
    collect_conditions_rec(op, dest);

  if(is_condition(src) && !src.is_constant())
    dest.insert(src); 
}

/*******************************************************************\

Function: collect_conditions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::set<exprt> collect_conditions(const exprt &src)
{
  std::set<exprt> result;
  collect_conditions_rec(src, result);
  return result;
}

/*******************************************************************\

Function: collect_conditions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::set<exprt> collect_conditions(const goto_programt::const_targett t)
{
  switch(t->type)
  {
  case GOTO:
  case ASSERT:
    return collect_conditions(t->guard);
  
  case ASSIGN:
  case FUNCTION_CALL:
    return collect_conditions(t->code);
    
  default:;
  }
  
  return std::set<exprt>();
}

/*******************************************************************\

Function: collect_operands

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void collect_operands(const exprt &src, std::vector<exprt> &dest)
{
  for(const exprt &op : src.operands())
  {
    if(op.id()==src.id())
      collect_operands(op, dest);
    else
      dest.push_back(op);
  }
}

/*******************************************************************\

Function: collect_mcdc_controlling_rec

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void collect_mcdc_controlling_rec(
  const exprt &src,
  const std::vector<exprt> &conditions,
  std::set<exprt> &result)
{
  if(src.id()==ID_and ||
     src.id()==ID_or)
  {
    std::vector<exprt> operands;
    collect_operands(src, operands);

    if(operands.size()==1)
    {
      exprt e=*operands.begin();
      collect_mcdc_controlling_rec(e, conditions, result);
    }
    else if(!operands.empty())
    {

      for(unsigned i=0; i<operands.size(); i++)
      {

        const exprt op=operands[i];
      
        if(is_condition(op))
        {

          if(src.id()==ID_or)
          {
            std::vector<exprt> others1, others2;
        	//others.reserve(operands.size()-1);
            if(not conditions.empty())
            {
              others1.push_back(conjunction(conditions));
              others2.push_back(conjunction(conditions));
            }

            for(unsigned j=0; j<operands.size(); j++)
            {
              others1.push_back(not_exprt(operands[j]));
        	  if(i!=j)
              {
        	    others2.push_back(not_exprt(operands[j]));
              }
        	  else
        	    others2.push_back((operands[j]));
            }


        	exprt c1=conjunction(others1);
        	exprt c2=conjunction(others2);
        	result.insert(c1);
        	result.insert(c2);
        	continue;
          }

          std::vector<exprt> o=operands;
        
          // 'o[i]' needs to be true and false
          std::vector<exprt> new_conditions=conditions;
          new_conditions.push_back(conjunction(o));
          result.insert(conjunction(new_conditions));

          o[i].make_not();
          new_conditions.back()=conjunction(o);
          result.insert(conjunction(new_conditions));
        }
        else
        {
          std::vector<exprt> others;
          others.reserve(operands.size()-1);

          for(unsigned j=0; j<operands.size(); j++)
            if(i!=j)
            {
              if(src.id()==ID_or)
                others.push_back(not_exprt(operands[j]));
              else
                others.push_back(operands[j]);
            }
            
          exprt c=conjunction(others);
          std::vector<exprt> new_conditions=conditions;
          new_conditions.push_back(c);

          collect_mcdc_controlling_rec(op, new_conditions, result);
        }
      }
    }
  }
  else if(src.id()==ID_not)
  {
    exprt e=to_not_expr(src).op();
    collect_mcdc_controlling_rec(e, conditions, result);
  }
}

/*******************************************************************\

Function: collect_mcdc_controlling

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::set<exprt> collect_mcdc_controlling(
  const std::set<exprt> &decisions)
{
  std::set<exprt> result;
  
  for(const auto &d : decisions)
  {
    collect_mcdc_controlling_rec(d, { }, result);
  }
  return result;
}

std::set<exprt> conjunction(const std::set<exprt> &res,
		                    const std::vector<exprt> &operands,
							const int i)
{
	std::set<exprt> result;
	for(auto &y : res)
    {
      std::vector<exprt> others;
      for(unsigned j=0; j<operands.size(); j++)
        if(i!=j)
        {
          others.push_back(operands[j]);
        }
      others.push_back(y);
      exprt c=conjunction(others);
      result.insert(c);
    }
	return result;
}

std::set<exprt> collect_mcdc_controlling_nested(
  const std::set<exprt> &decisions)
{
  std::set<exprt> controlling = collect_mcdc_controlling(decisions);
  std::set<exprt> result;
  for(auto &src : controlling)
  {
    std::set<exprt> s1, s2;
	s1.insert(src);
	while(true) //dual-loop structure to eliminate complex non-conditional terms
	{
	  bool changed=false;
	  for(auto &x : s1)
	  {
	    std::vector<exprt> operands;
	    collect_operands(x, operands);
	    for(int i=0; i<operands.size(); i++)
	    {
	      std::set<exprt> res;
	      if(operands[i].id()==ID_not)
	      {
	        exprt no=operands[i].op0();
	        if(not is_condition(no))
	      	{
	          changed=true;
	      	  std::set<exprt> dec_no;
	      	  dec_no.insert(no);
	      	  res=collect_mcdc_controlling(dec_no);
	      	}
	      }
	      else if(not is_condition(operands[i]))
	      {
	        changed=true;
	      	std::set<exprt> dec_no;
	      	dec_no.insert(operands[i]);
	      	res=collect_mcdc_controlling(dec_no);
	      }

	      std::set<exprt> co=conjunction(res, operands, i);
	      s2.insert(co.begin(), co.end());
	      if(res.size() > 0) break;
	    }
	    if(not changed) s2.insert(x);
	  }
	  s1=s2;
	  if(not changed) {break;}
	  s2.clear();
	}
	result.insert(s1.begin(), s1.end());
  }

  return result;
}


/*******************************************************************\

Function: negate an expr (<, <=, ==, >=, >) with the AUTOSAC semantic

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::set<exprt> autosac_atomic_negate(const exprt &src)
{
  /**
   * This is an customized approach to "negate" exprts
   */
  std::set<exprt> result;
  if( src.id()==ID_equal)
  {
    // the negation of "==" is "<" and ">"
    exprt e1(ID_lt);
    e1.type().id(src.type().id());
    e1.operands().push_back(src.op0());
    e1.operands().push_back(src.op1());

    exprt e2(ID_gt);
    e2.type().id(src.type().id());
    e2.operands().push_back(src.op0());
    e2.operands().push_back(src.op1());

    result.insert(e1);
    result.insert(e2);


  }
  else if(src.id()==ID_lt)
  {
    //the negation of "<" should be "==" and ">"
    exprt e1(ID_equal);
    e1.type().id(src.type().id());
    e1.operands().push_back(src.op0());
    e1.operands().push_back(src.op1());

    exprt e2(ID_gt);
    e2.type().id(src.type().id());
    e2.operands().push_back(src.op0());
    e2.operands().push_back(src.op1());

    result.insert(e1);
    result.insert(e2);

  }
  else if(src.id()==ID_le)
  {
    //the negation of "<=" should be ">"
    exprt e1(ID_gt);
    e1.type().id(src.type().id());
    e1.operands().push_back(src.op0());
    e1.operands().push_back(src.op1());

    result.insert(e1);


  }
  else if(src.id()==ID_ge)
  {
    //the negation of ">=" should be "<"
    exprt e1(ID_lt);
    e1.type().id(src.type().id());
    e1.operands().push_back(src.op0());
    e1.operands().push_back(src.op1());

    result.insert(e1);
  }
  else if(src.id()==ID_gt)
  {
    //the negation of ">" should be "==" and "<"
    exprt e1(ID_equal);
    e1.type().id(src.type().id());
    e1.operands().push_back(src.op0());
    e1.operands().push_back(src.op1());

    exprt e2(ID_lt);
    e2.type().id(src.type().id());
    e2.operands().push_back(src.op0());
    e2.operands().push_back(src.op1());

    result.insert(e1);
    result.insert(e2);
  }
 /** else
  {
    exprt neg(src);
    neg.make_not();
    result.insert(neg);
  }
**/
  return result;
}


signed sign_of_exprt(const exprt &e, const exprt E)
{
  // to check the sign of "e" inside a compact boolean
  // formula "E"

  auto &ops = E.operands();
  for(auto &x : ops)
  {
	exprt y(x);
	if(y == e) return 1;
	else if(y.id()==ID_not)
	{
	  y.make_not();
	  if(y==e) return -1;
	  if(not is_condition(y))
	  {
	    int res = sign_of_exprt(e,y);
	    if(res!=0) return res;
	  }
	}
	else if(not is_condition(y))
	{
	  int res = sign_of_exprt(e,y);
	  if(res!=0) return res;
	}
  }
  return 0;
}

void remove_redundancy(std::set<exprt> &exprts)
{
  std::set<exprt> conditions;
  for(auto &x: exprts)
  {
    std::set<exprt> new_conditions = collect_conditions(x);
    conditions.insert(new_conditions.begin(), new_conditions.end());
  }

  std::set<exprt> new_exprts;
  for(auto &x: exprts)
  {

    bool red=false;
    for(auto &y: new_exprts)
    {
      bool iden = true;
      for(auto &c : conditions)
        if(sign_of_exprt(c, x) != sign_of_exprt(c, y))
        {
          iden = false;
          break;
        }
      if(iden)
      {
        red = true;
        break;
      }
    }
    if(not red) new_exprts.insert(x);
  }
  exprts = new_exprts;
}

std::set<exprt> autosac_expand(const exprt &src)
{
  std::set<exprt> result;

  std::set<exprt> s1, s2;
  s1.insert(src);

  while(true) // dual-loop structure to expand negations
  {
	bool changed=false;
	for(auto &x : s1)
	{

      std::vector<exprt> operands;
      collect_operands(x, operands);
      for(int i=0; i<operands.size(); i++)
      {
    	std::set<exprt> res;
    	if(operands[i].id()==ID_not)
    	{
    	  exprt no=operands[i].op0();
    	  if((no.id()==ID_equal or no.id()==ID_lt or no.id()==ID_le or no.id()==ID_ge or no.id()==ID_gt))
    	  {
    	    changed=true;
    		res=autosac_atomic_negate(no);
    	  }

    	}

	    std::set<exprt> co=conjunction(res, operands, i);
	    s2.insert(co.begin(), co.end());
	    if(res.size() > 0) break;
      }
      if(not changed) s2.insert(x);
    }
	if(not changed)
	  return s1;
	s1=s2;
	s2.clear();
  }
}



        
/*******************************************************************\

Function: collect_decisions_rec

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void collect_decisions_rec(const exprt &src, std::set<exprt> &dest)
{
  if(src.id()==ID_address_of)
  {
    return;
  }

  if(src.type().id()==ID_bool)
  {
    if(src.is_constant())
    {
      // ignore me
    }
    else if(src.id()==ID_not)
    {
      collect_decisions_rec(src.op0(), dest);
    }
    else
    {
      dest.insert(src); 
    }
  }
  else
  {
    for(const auto & op : src.operands())
      collect_decisions_rec(op, dest);
  }
}

/*******************************************************************\

Function: collect_decisions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::set<exprt> collect_decisions(const exprt &src)
{
  std::set<exprt> result;
  collect_decisions_rec(src, result);
  return result;
}

/*******************************************************************\

Function: collect_decisions

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::set<exprt> collect_decisions(const goto_programt::const_targett t)
{
  switch(t->type)
  {
  case GOTO:
  case ASSERT:
    return collect_decisions(t->guard);
  
  case ASSIGN:
  case FUNCTION_CALL:
    return collect_decisions(t->code);
    
  default:;
  }
  
  return std::set<exprt>();
}

/*******************************************************************\

Function: instrument_cover_goals

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instrument_cover_goals(
  const symbol_tablet &symbol_table,
  goto_programt &goto_program,
  coverage_criteriont criterion)
{
  const namespacet ns(symbol_table);
  basic_blockst basic_blocks(goto_program);
  std::set<unsigned> blocks_done;
  
  // ignore if built-in library
  if(!goto_program.instructions.empty() &&
     has_prefix(id2string(goto_program.instructions.front().source_location.get_file()),
                "<builtin-library-"))
    return;
  
  Forall_goto_program_instructions(i_it, goto_program)
  {
    switch(criterion)
    {
    case coverage_criteriont::ASSERTION:
      // turn into 'assert(false)' to avoid simplification
      if(i_it->is_assert())
      {
        i_it->guard=false_exprt();
        i_it->source_location.set_property_class("coverage");
      }
      break;
      
    case coverage_criteriont::COVER:
      // turn __CPROVER_cover(x) into 'assert(!x)'
      if(i_it->is_function_call())
      {
        const code_function_callt &code_function_call=
          to_code_function_call(i_it->code);
        if(code_function_call.function().id()==ID_symbol &&
           to_symbol_expr(code_function_call.function()).get_identifier()==
           "__CPROVER_cover" &&
           code_function_call.arguments().size()==1)
        {
          const exprt c=code_function_call.arguments()[0];
          std::string comment="condition `"+from_expr(ns, "", c)+"'";
          i_it->guard=not_exprt(c);
          i_it->type=ASSERT;
          i_it->code.clear();
          i_it->source_location.set_comment(comment);
          i_it->source_location.set_property_class("coverage");
        }
      }
      else if(i_it->is_assert())
        i_it->make_skip();
      break;
      
    case coverage_criteriont::LOCATION:
      if(i_it->is_assert())
        i_it->make_skip();

      {
        unsigned block_nr=basic_blocks[i_it];
        if(blocks_done.insert(block_nr).second)
        {
          std::string b=i2string(block_nr);
          std::string id=id2string(i_it->function)+"#"+b;
          source_locationt source_location=
            basic_blocks.source_location_map[block_nr];
          
          if(!source_location.get_file().empty() &&
             source_location.get_file()[0]!='<')
          {
            std::string comment="block "+b;
            goto_program.insert_before_swap(i_it);
            i_it->make_assertion(false_exprt());
            i_it->source_location=source_location;
            i_it->source_location.set_comment(comment);
            i_it->source_location.set_property_class("coverage");
            
            i_it++;
          }
        }
      }
      break;
    
    case coverage_criteriont::BRANCH:
      if(i_it->is_assert())
        i_it->make_skip();

      if(i_it==goto_program.instructions.begin())
      {
        // we want branch coverage to imply 'entry point of function'
        // coverage
        std::string comment=
          "function "+id2string(i_it->function)+" entry point";

        source_locationt source_location=i_it->source_location;

        goto_programt::targett t=goto_program.insert_before(i_it);
        t->make_assertion(false_exprt());
        t->source_location=source_location;
        t->source_location.set_comment(comment);
        t->source_location.set_property_class("coverage");
      }
    
      if(i_it->is_goto() && !i_it->guard.is_true())
      {
        std::string b=i2string(basic_blocks[i_it]);
        std::string true_comment=
          "function "+id2string(i_it->function)+" block "+b+" branch true";
        std::string false_comment=
          "function "+id2string(i_it->function)+" block "+b+" branch false";

        exprt guard=i_it->guard;
        source_locationt source_location=i_it->source_location;

        goto_program.insert_before_swap(i_it);
        i_it->make_assertion(not_exprt(guard));
        i_it->source_location=source_location;
        i_it->source_location.set_comment(true_comment);
        i_it->source_location.set_property_class("coverage");

        goto_program.insert_before_swap(i_it);
        i_it->make_assertion(guard);
        i_it->source_location=source_location;
        i_it->source_location.set_comment(false_comment);
        i_it->source_location.set_property_class("coverage");
        
        i_it++;
        i_it++;
      }
      break;
      
    case coverage_criteriont::CONDITION:
      if(i_it->is_assert())
        i_it->make_skip();

      // Conditions are all atomic predicates in the programs.
      {
        const std::set<exprt> conditions=collect_conditions(i_it);

        const source_locationt source_location=i_it->source_location;

        for(const auto & c : conditions)
        {
          const std::string c_string=from_expr(ns, "", c);
        
          const std::string comment_t="condition `"+c_string+"' true";
          goto_program.insert_before_swap(i_it);
          i_it->make_assertion(c);
          i_it->source_location=source_location;
          i_it->source_location.set_comment(comment_t);
          i_it->source_location.set_property_class("coverage");

          const std::string comment_f="condition `"+c_string+"' false";
          goto_program.insert_before_swap(i_it);
          i_it->make_assertion(not_exprt(c));
          i_it->source_location=source_location;
          i_it->source_location.set_comment(comment_f);
          i_it->source_location.set_property_class("coverage");
        }
        
        for(unsigned i=0; i<conditions.size()*2; i++)
          i_it++;
      }
      break;
    
    case coverage_criteriont::DECISION:
      if(i_it->is_assert())
        i_it->make_skip();

      // Decisions are maximal Boolean combinations of conditions.
      {
        const std::set<exprt> decisions=collect_decisions(i_it);

        const source_locationt source_location=i_it->source_location;

        for(const auto & d : decisions)
        {
          const std::string d_string=from_expr(ns, "", d);
        
          const std::string comment_t="decision `"+d_string+"' true";
          goto_program.insert_before_swap(i_it);
          i_it->make_assertion(d);
          i_it->source_location=source_location;
          i_it->source_location.set_comment(comment_t);
          i_it->source_location.set_property_class("coverage");

          const std::string comment_f="decision `"+d_string+"' false";
          goto_program.insert_before_swap(i_it);
          i_it->make_assertion(not_exprt(d));
          i_it->source_location=source_location;
          i_it->source_location.set_comment(comment_f);
          i_it->source_location.set_property_class("coverage");
        }
        
        for(unsigned i=0; i<decisions.size()*2; i++)
          i_it++;
      }
      break;

    case coverage_criteriont::AUTOSAC:
    case coverage_criteriont::MCDC:

      // 1. Each entry and exit point is invoked
      // 2. Each decision takes every possible outcome
      // 3. Each condition in a decision takes every possible outcome
      // 4. Each condition in a decision is shown to independently
      //    affect the outcome of the decision.
      {
        bool autosac=(criterion==coverage_criteriont::AUTOSAC);
    	if(i_it->is_assert())
    	  if(not autosac)
    	    i_it->make_skip();
        const std::set<exprt> conditions=collect_conditions(i_it);
        const std::set<exprt> decisions=collect_decisions(i_it);

        std::set<exprt> both;
        std::set_union(conditions.begin(), conditions.end(),
                       decisions.begin(), decisions.end(),
                       inserter(both, both.end()));


        const source_locationt source_location=i_it->source_location;

        for(const auto & p : both)
        {
          bool is_decision=decisions.find(p)!=decisions.end();
          bool is_condition=conditions.find(p)!=conditions.end();
          
          std::string description=
            (is_decision && is_condition)?"decision/condition":
            is_decision?"decision":"condition";
            
          std::string p_string=from_expr(ns, "", p);
        

          std::string comment_t=description+" `"+p_string+"' true";
          goto_program.insert_before_swap(i_it);
          i_it->make_assertion(p);
          i_it->source_location=source_location;
          i_it->source_location.set_comment(comment_t);
          i_it->source_location.set_property_class("coverage");

          std::string comment_f=description+" `"+p_string+"' false";
          goto_program.insert_before_swap(i_it);
          i_it->make_assertion(not_exprt(p));
          i_it->source_location=source_location;
          i_it->source_location.set_comment(comment_f);
          i_it->source_location.set_property_class("coverage");

          std::cout << "comment t/f" << comment_t << "/" << comment_f << std::endl;
        }
        
        std::set<exprt> controlling;
        controlling=collect_mcdc_controlling_nested(decisions);
        remove_redundancy(controlling);

        if(autosac)
        {
          std::set<exprt> controlling2;
          for(auto &x: controlling)
          {
            std::set<exprt> res=autosac_expand(x);
            controlling2.insert(res.begin(), res.end());
          }
          controlling=controlling2;
          remove_redundancy(controlling);
        }


        for(const auto & p : controlling)
        {
          std::string p_string=from_expr(ns, "", p);

          std::string description=
            "MC/DC independence condition `"+p_string+"'";
            
          goto_program.insert_before_swap(i_it);
          if(autosac)
        	  i_it->make_assertion(not_exprt(p));
          else
            i_it->make_assertion(p);
          i_it->source_location=source_location;
          i_it->source_location.set_comment(description);
          i_it->source_location.set_property_class("coverage");
        }
        
        for(unsigned i=0; i<both.size()*2+controlling.size(); i++)
          i_it++;
      }
      break;

    case coverage_criteriont::PATH:
      if(i_it->is_assert())
        i_it->make_skip();
      break;
    
    default:;
    }
  }

}

/*******************************************************************\

Function: instrument_cover_goals

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void instrument_cover_goals(
  const symbol_tablet &symbol_table,
  goto_functionst &goto_functions,
  coverage_criteriont criterion)
{
  Forall_goto_functions(f_it, goto_functions)
  {
    if(f_it->first==ID__start ||
       f_it->first=="__CPROVER_initialize")
      continue;
      
    instrument_cover_goals(symbol_table, f_it->second.body, criterion);
  }
}
