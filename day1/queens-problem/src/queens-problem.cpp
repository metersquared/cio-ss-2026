#include <format>
#include <iostream>
#include <memory>
#include <objscip/objscip.h>
#include <objscip/objscipdefplugins.h>
#include <print>
#include <vector>

#include "utils.hpp"

constexpr int chessboard_size = 8;
constexpr int idx(int row, int col) { return row * chessboard_size + col; };
constexpr int diag_idx(int row, int col, int shift) {
  return idx(row + shift, col + shift);
};
constexpr int adiag_idx(int row, int col, int shift) {
  return idx(row + shift, col - shift);
};

struct SCIPDeleter {
  void operator()(SCIP *scip) const { SCIPfree(&scip); }
};
using SCIPPtr = std::unique_ptr<SCIP, SCIPDeleter>;

struct VarDeleter {
  SCIP *scip;
  VarDeleter(SCIP *scip) : scip(scip) {}
  void operator()(SCIP_VAR *var) const { SCIPreleaseVar(scip, &var); }
};
using VarPtr = std::unique_ptr<SCIP_VAR, VarDeleter>;

struct ConstDeleter {
  SCIP *scip;
  ConstDeleter(SCIP *scip) : scip(scip) {}
  void operator()(SCIP_CONS *cons) const { SCIPreleaseCons(scip, &cons); }
};
using ConstPtr = std::unique_ptr<SCIP_CONS, ConstDeleter>;

void add_no_good(SCIP *scip, std::vector<SCIP_Real> &sol_values,
                 std::vector<VarPtr> &vars, int solution_number) {
  ConstDeleter constDeleter(scip);
  ConstPtr cons(nullptr, constDeleter);
  CALL_CHECK(SCIPcreateConsBasicLinear(
      scip, std::out_ptr(cons),
      std::format("no_good_{}", solution_number).c_str(), 0, nullptr, nullptr,
      -SCIPinfinity(scip), chessboard_size - 1));
  for (int i = 0; i < chessboard_size; ++i) {
    for (int j = 0; j < chessboard_size; ++j) {
      if (sol_values[idx(i, j)] > 0.5) {
        CALL_CHECK(
            SCIPaddCoefLinear(scip, cons.get(), vars[idx(i, j)].get(), 1.0));
      }
    }
  }
  CALL_CHECK(SCIPaddCons(scip, cons.get()));
}

void visualize_solution(std::vector<SCIP_Real> &sol_values, int solution_number){
  std::cout<<"Solution number "<<solution_number<<":"<<std::endl;
  for (int i = 0; i < chessboard_size; ++i) {
    std::string row_string ="";
    for (int j = 0; j < chessboard_size; ++j) {
      if (sol_values[idx(i, j)] > 0.5) {
        row_string=row_string+"X";
      }
      else {
        row_string=row_string+"-";
      }
    }
    std::cout<<row_string<<std::endl;
  }
}

int main() {
  SCIPPtr scip;
  CALL_CHECK(SCIPcreate(std::out_ptr(scip)));
  CALL_CHECK(SCIPincludeDefaultPlugins(scip.get()));
  CALL_CHECK(SCIPsetIntParam(scip.get(), "display/verblevel", 1));
  CALL_CHECK(SCIPcreateProbBasic(scip.get(), "ExampleMIP"));
  CALL_CHECK(SCIPsetObjsense(scip.get(), SCIP_OBJSENSE_MAXIMIZE));
  const SCIP_Real inf = SCIPinfinity(scip.get());

  VarDeleter var_deleter(scip.get());
  std::vector<VarPtr> vars;
  SCIP_Real lb = 0;
  SCIP_Real ub = 1;
  SCIP_Real obj = 1;
  SCIP_VARTYPE type = SCIP_VARTYPE_BINARY;

  vars.reserve(chessboard_size * chessboard_size);
  for (int i = 0; i < chessboard_size; ++i) {
    for (int j = 0; j < chessboard_size; ++j) {
      VarPtr var(nullptr, var_deleter);
      CALL_CHECK(SCIPcreateVarBasic(scip.get(), std::out_ptr(var),
                                    std::format("x_({},{})", i, j).c_str(), lb,
                                    ub, obj, type))
      CALL_CHECK(SCIPaddVar(scip.get(), var.get()));
      vars.push_back(std::move(var));
    }
  }

  // Constraints
  ConstDeleter cons_deleter(scip.get());

  // Row and column constraints
  std::vector<ConstPtr> row_conss;
  std::vector<ConstPtr> col_conss;

  for (int i = 0; i < chessboard_size; ++i) {
    ConstPtr c_row(nullptr, cons_deleter);
    CALL_CHECK(SCIPcreateConsBasicLinear(scip.get(), std::out_ptr(c_row),
                                         std::format("row_{}", i).c_str(), 0,
                                         nullptr, nullptr, 1, 1));
    ConstPtr c_col(nullptr, cons_deleter);
    CALL_CHECK(SCIPcreateConsBasicLinear(scip.get(), std::out_ptr(c_col),
                                         std::format("col_{}", i).c_str(), 0,
                                         nullptr, nullptr, 1, 1));
    for (int j = 0; j < chessboard_size; ++j) {
      CALL_CHECK(
          SCIPaddCoefLinear(scip.get(), c_row.get(), vars[idx(i, j)].get(), 1))
      CALL_CHECK(
          SCIPaddCoefLinear(scip.get(), c_col.get(), vars[idx(j, i)].get(), 1))
    }
    CALL_CHECK(SCIPaddCons(scip.get(), c_row.get()));
    CALL_CHECK(SCIPaddCons(scip.get(), c_col.get()));
  }

  // Diagonal constraints
  std::vector<ConstPtr> diag_conss;
  std::vector<ConstPtr> adiag_conss;

  for (int i=-7; i<chessboard_size; ++i) {
    int init_row_diag, init_row_adiag,init_col_diag, init_col_adiag;
    if (i<0) {
      init_row_diag = std::abs(i);
      init_col_diag = 0;

      init_row_adiag = init_row_diag;
      init_col_adiag = chessboard_size-1;
    }
    else if (i>0) {
      init_row_diag = 0;
      init_col_diag = std::abs(i);
      
      init_row_adiag = 0;
      init_col_adiag = chessboard_size - i;
    }
    else {
      init_row_diag=0;
      init_col_diag=0;
      
      init_row_adiag = init_row_diag;
      init_col_adiag=chessboard_size-1;
    }

    ConstPtr c_diag(nullptr, cons_deleter);
    CALL_CHECK(SCIPcreateConsBasicLinear(scip.get(), std::out_ptr(c_diag),
                                         std::format("diag_{}", i).c_str(), 0,
                                         nullptr, nullptr, -inf, 1));
    ConstPtr c_adiag(nullptr, cons_deleter);
    CALL_CHECK(SCIPcreateConsBasicLinear(scip.get(), std::out_ptr(c_adiag),
                                         std::format("antidiag_{}", i).c_str(), 0,
                                         nullptr, nullptr, -inf, 1));
    
    for (int j=0; j<chessboard_size-std::abs(i); ++j) {
      CALL_CHECK(
          SCIPaddCoefLinear(scip.get(), c_diag.get(), vars[diag_idx(init_row_diag, init_col_diag, j)].get(), 1))
      CALL_CHECK(
          SCIPaddCoefLinear(scip.get(), c_adiag.get(), vars[adiag_idx(init_row_adiag, init_col_adiag, j)].get(), 1))
    }
    CALL_CHECK(SCIPaddCons(scip.get(), c_diag.get()));
    CALL_CHECK(SCIPaddCons(scip.get(), c_adiag.get()));
  }

  std::vector<SCIP_Real> solution_values(92,0);
  // Print problem as "cip" to stdout for debug
  CALL_CHECK(SCIPprintOrigProblem(scip.get(), nullptr, "cip", false));

  for (int i = 0; i< 92; ++i) {
    SCIPfreeTransform(scip.get());

    if (i!=0) {
      add_no_good(scip.get(), solution_values, vars, i);
    }

    // Solve the problem
    CALL_CHECK(SCIPsolve(scip.get()));

    // Check if the solution is optimal, print the optimal objective value along with the solution 
    if (SCIPgetStatus(scip.get())==SCIP_STATUS_OPTIMAL){
        auto sol = SCIPgetBestSol(scip.get());
        std::cout << "Obj Value:"<<SCIPgetSolOrigObj(scip.get(), sol)<< std::endl;
        
        for (int i=0; i<64; ++i) {
        solution_values[i]=SCIPgetSolVal(scip.get(), sol, vars[i].get());
        }
        visualize_solution(solution_values,i+1);
    }
  }
}