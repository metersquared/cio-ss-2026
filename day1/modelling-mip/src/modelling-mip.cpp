#include <format>
#include <iostream>
#include <memory>
#include <objscip/objscip.h>
#include <objscip/objscipdefplugins.h>
#include <print>
#include <vector>

#include "utils.hpp"

// Wrapper for SCIP -> SCIPPtr and SCIPDeleter
struct SCIPDeleter{
    void operator()(SCIP* p){
        SCIPfree(&p);
    }
};
using SCIPPtr = std::unique_ptr<SCIP, SCIPDeleter>;

// Wrapper for SCIP_VAR -> VarPtr and VarDeleter
struct VarDeleter{
    SCIP *scip;
    VarDeleter(SCIP*scip):scip(scip){}
    void operator()(SCIP_VAR* p){
        SCIPreleaseVar(scip, &p);
    }
};
using VarPtr = std::unique_ptr<SCIP_VAR, VarDeleter>;

// Wrapper for SCIP_CONS -> ConstPtr and ConstDeleter
struct ConsDeleter{
    SCIP *scip;
    ConsDeleter(SCIP*scip):scip(scip){}
    void operator()(SCIP_CONS* p){
        SCIPreleaseCons(scip, &p);
    }
};
using ConsPtr = std::unique_ptr<SCIP_CONS, ConsDeleter>;

int main(){
    // Create SCIP pointer, initialize and set up the problem
    SCIPPtr scip;
    CALL_CHECK(SCIPcreate(std::out_ptr(scip)));
    CALL_CHECK(SCIPincludeDefaultPlugins(scip.get()));
    CALL_CHECK(SCIPcreateProbBasic(scip.get(), "example"));
    
    //Set objective sense
    CALL_CHECK(SCIPsetObjsense(scip.get(), SCIP_OBJSENSE_MAXIMIZE));

    // Create Variables
    const SCIP_Real inf = SCIPinfinity(scip.get());
    VarDeleter var_deleter(scip.get());
    std::vector<VarPtr> vars;
    SCIP_Real lb[] = {0, 0, 0, 2};
    SCIP_Real ub[] = {40, inf, inf, 3};
    SCIP_Real obj[] = {1,2,3,1};
    std::string names[] = {"x1", "x2","x3","x4"};
    SCIP_VARTYPE types[] = {SCIP_VARTYPE_CONTINUOUS, SCIP_VARTYPE_CONTINUOUS, SCIP_VARTYPE_CONTINUOUS, SCIP_VARTYPE_INTEGER};

    vars.reserve(4);
    for (int i= 0; i<4; i++){
        VarPtr var(nullptr,var_deleter);
        CALL_CHECK(SCIPcreateVarBasic(scip.get(), std::out_ptr(var), names[i].c_str(), lb[i], ub[i],obj[i], types[i]));
        CALL_CHECK(SCIPaddVar(scip.get(), var.get()));
        vars.push_back(std::move(var));
    }

    // Create Constraints
    ConsDeleter cons_deleter(scip.get());
    std::vector<ConsPtr> conss;
    SCIP_Real lhs[]= {-inf,-inf, 0};
    SCIP_Real rhs[]= {20,30,0};
    std::vector<std::vector<SCIP_Real>> coeffs = {{-1,1,1,10},{1,-3,1},{1,-3.5}};
    std::vector<std::vector<int>> indices = {{0,1,2,3},{0,1,2},{1,3}};
    std::string cons_name[] = {"c1","c2","c3"};

    for (int i=0; i<3;++i){
        ConsPtr c(nullptr,cons_deleter);
        CALL_CHECK(SCIPcreateConsBasicLinear(scip.get(),std::out_ptr(c),cons_name[i].c_str(), 0, nullptr,nullptr,lhs[i],rhs[i]));
        for (int j=0; j<std::ssize(coeffs[i]); ++j) {
            CALL_CHECK(SCIPaddCoefLinear(scip.get(), c.get(), vars[indices[i][j]].get(), coeffs[i][j]))
        }
        CALL_CHECK(SCIPaddCons(scip.get(), c.get()));
    }

    // Print problem as "cip" to stdout for debug
    CALL_CHECK(SCIPprintOrigProblem(scip.get(), nullptr, "cip", false));

    // Solve the problem
    CALL_CHECK(SCIPsolve(scip.get()))


    // Check if the solution is optimal, print the optimal objective value along with the solution 
    if (SCIPgetStatus(scip.get())==SCIP_STATUS_OPTIMAL){
        auto sol = SCIPgetBestSol(scip.get());
        std::cout << "Obj Value:"<<SCIPgetSolOrigObj(scip.get(), sol)<< std::endl;
        std::cout << "Solution:"<< std::endl;
        for (int i=0; i<4; ++i) {
        std::cout<< "x"<< i<<": "<< SCIPgetSolVal(scip.get(), sol, vars[i].get())<<std::endl;
        }
    }
}