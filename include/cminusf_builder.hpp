#ifndef _CMINUSF_BUILDER_HPP_
#define _CMINUSF_BUILDER_HPP_
#include "BasicBlock.h"
#include "Constant.h"
#include "Function.h"
#include "IRBuilder.h"
#include "Module.h"
#include "Type.h"
#include "ast.hpp"
#include "logging.hpp"
#include <map>
#include <stack>
#include <utility>
#include <functional>
#include <algorithm>

// use these macros to get constant value

#define MOD builder->get_module()

// Macros for Type
#define GET_INT32 Type::get_int32_type(MOD)
#define GET_FLOAT Type::get_float_type(MOD)
#define GET_VOID Type::get_void_type(MOD)
#define GET_BOOL Type::get_int1_type(MOD)

#define CONST_INT(num) \
    (ConstantInt::get(num, MOD))
#define CONST_FP(num) \
    (ConstantFP::get(num, MOD))

#define GET_CONST(astNum) \
    ((astNum).type == TYPE_INT ? (Value *)CONST_INT((astNum).i_val) : (Value *)CONST_FP((astNum).f_val))
#define CONST_ZERO(type) \
    ConstantZero::get(type, MOD)

#define INIT(Ty) ConstantZero::get((Ty), MOD)

class Scope
{
public:
    // enter a new scope
    void enter()
    {
        inner.push_back({});
    }

    // exit a scope
    void exit()
    {
        inner.pop_back();
    }

    bool in_global()
    {
        return inner.size() == 1;
    }

    // push a name to scope
    // return true if successful
    // return false if this name already exits
    bool push(std::string name, Value *val)
    {
        auto result = inner[inner.size() - 1].insert({name, val});
        return result.second;
    }

    Value *find(std::string name)
    {
        for (auto s = inner.rbegin(); s != inner.rend(); s++)
        {
            auto iter = s->find(name);
            if (iter != s->end())
            {
                return iter->second;
            }
        }

        return nullptr;
    }

private:
    std::vector<std::map<std::string, Value *>> inner;
};

class CminusfBuilder : public ASTVisitor
{
public:
    CminusfBuilder()
    {
        module = std::unique_ptr<Module>(new Module("Cminus code"));
        builder = new IRBuilder(nullptr, module.get());
        auto TyVoid = Type::get_void_type(module.get());
        auto TyInt32 = Type::get_int32_type(module.get());
        auto TyFloat = Type::get_float_type(module.get());

        auto input_type = FunctionType::get(TyInt32, {});
        auto input_fun =
            Function::create(
                input_type,
                "input",
                module.get());

        std::vector<Type *> output_params;
        output_params.push_back(TyInt32);
        auto output_type = FunctionType::get(TyVoid, output_params);
        auto output_fun =
            Function::create(
                output_type,
                "output",
                module.get());

        std::vector<Type *> output_float_params;
        output_float_params.push_back(TyFloat);
        auto output_float_type = FunctionType::get(TyVoid, output_float_params);
        auto output_float_fun =
            Function::create(
                output_float_type,
                "outputFloat",
                module.get());

        auto neg_idx_except_type = FunctionType::get(TyVoid, {});
        auto neg_idx_except_fun =
            Function::create(
                neg_idx_except_type,
                "neg_idx_except",
                module.get());

        scope.enter();
        scope.push("input", input_fun);
        scope.push("output", output_fun);
        scope.push("outputFloat", output_float_fun);
        scope.push("neg_idx_except", neg_idx_except_fun);
    }

    std::unique_ptr<Module> getModule()
    {
        return std::move(module);
    }

private:
    using ConvertorFuncType = std::function<void(Value *&)>;
    using CompFuncType = std::function<Value *(Value *left, Value *right)>;
    using AddFuncType = std::function<Value *(Value *left, Value *right)>;
    using MulFuncType = std::function<Value *(Value *left, Value *right)>;

    virtual void visit(ASTProgram &) override final;
    virtual void visit(ASTNum &) override final;
    virtual void visit(ASTVarDeclaration &) override final;
    virtual void visit(ASTFunDeclaration &) override final;
    virtual void visit(ASTParam &) override final;
    virtual void visit(ASTCompoundStmt &) override final;
    virtual void visit(ASTExpressionStmt &) override final;
    virtual void visit(ASTSelectionStmt &) override final;
    virtual void visit(ASTIterationStmt &) override final;
    virtual void visit(ASTReturnStmt &) override final;
    virtual void visit(ASTAssignExpression &) override final;
    virtual void visit(ASTSimpleExpression &) override final;
    virtual void visit(ASTAdditiveExpression &) override final;
    virtual void visit(ASTVar &) override final;
    virtual void visit(ASTTerm &) override final;
    virtual void visit(ASTCall &) override final;

    IRBuilder *builder;
    Scope scope;
    std::unique_ptr<Module> module;

    // See `group discussion.md` for more info
    std::map<Type *, int> typeOrderRank;
    std::map<std::pair<Type *, enum RelOp>, CompFuncType> compFuncTable;
    std::map<std::pair<Type *, enum AddOp>, AddFuncType> addFuncTable;
    std::map<std::pair<Type *, enum MulOp>, MulFuncType> mulFuncTable;
    std::map<std::pair<Type *, Type *>, ConvertorFuncType> compulsiveTypeConvertTable;

    Function *lastEnteredFun = nullptr;
    bool enteredFun = true;

    bool isTerminalStmt = false;
    std::vector<std::shared_ptr<ASTParam>> *params = nullptr;
    std::list<Argument *>::iterator curArg;

    // Use stack to evaluate expressions
    std::stack<Value *> cal_stack;

    size_t label_name_cnt = 0;

    /**
     * @brief Convert `enum CminusType` to `Type *`
     * @param CminusType a `enum CminusType` value
     * @return  a `Type *` value. 
     * 
     */
    Type *CminusTypeConvertor(enum CminusType c)
    {
        switch (c)
        {
        case TYPE_INT:
            return GET_INT32;
            break;
        case TYPE_FLOAT:
            return GET_FLOAT;
            break;
        case TYPE_VOID:
            return GET_VOID;
            break;
        default:
            break;
        }
    }

    Value *getArrOrPtrAddr(Value *var, Value *index)
    {
        Value *obj_addr;
        if (var->get_type()->get_pointer_element_type()->is_array_type()) // is array
            obj_addr = builder->create_gep(var, {CONST_INT(0), index});
        else // is pointer
        {
            var = builder->create_load(var);
            obj_addr = builder->create_gep(var, {index});
        }
        return obj_addr;
    }

    /**
     * @brief Compulsively convert value `origin` to `target` type.
     * 
     * e.g. If `a` is of type `int` and has a non-zero value, 
     * `compulsiveTypeConvert(a, GET_BOOL)` will change `a` to 
     * a bool type with value true. 
     * 
     */
    void compulsiveTypeConvert(Value *&origin, Type *target)
    {
        if (origin->get_type() != target)
            compulsiveTypeConvertTable[{origin->get_type(), target}](origin);
    }

    /**
     * @brief Convert left/right to the same type
     * 
     * e.g. If left is of type bool, right is of type int, 
     * then left is converted to type int with value 1. 
     * 
     * The rank of types are as following: 
     * 1. float
     * 2. int
     * 3. bool
     * 
     * The conversion can only be performed from a lower-rank type
     * to a higher-rank type. 
     * 
     * @param left 
     * @param right 
     * @return Type* The type of left and right after conversion.
     */
    Type *augmentTypeConvert(Value *&left, Value *&right)
    {
        auto resType = std::max(left->get_type(), right->get_type(), [&](Type *l, Type *r)
                                { return typeOrderRank[l] < typeOrderRank[r]; });
        compulsiveTypeConvert(left, resType);
        compulsiveTypeConvert(right, resType);
        return resType;
    }

    /**
     * @brief Same as `Type *augmentTypeConvert(Value *&left, Value *&right)`
     * with the exception that, after conversion, the rank of
     * the type of left&right cannot be lower than min.
     * 
     * e.g. if left and right are both of type bool, 
     * after running `compulsiveTypeConvert(left, right, GET_INT)`,
     * both of them are converted to int. 
     * @return Type* 
     */
    Type *augmentTypeConvert(Value *&left, Value *&right, Type *min)
    {
        auto resType = std::max({left->get_type(), right->get_type(), min}, [&](Type *l, Type *r)
                                { return typeOrderRank[l] < typeOrderRank[r]; });
        compulsiveTypeConvert(left, resType);
        compulsiveTypeConvert(right, resType);
        return resType;
    }

    BasicBlock *newBasicBlock()
    {
        return BasicBlock::create(MOD, std::to_string(label_name_cnt++), lastEnteredFun);
    }

    auto get_valid_param_ty(IRBuilder *builder, ASTParam &param)
    {
        Type *ty;
        if (param.isarray)
            ty = PointerType::get(CminusTypeConvertor(param.type));
        else
            ty = CminusTypeConvertor(param.type);
        return ty;
    }

    Value *get_arr_elem_addr(IRBuilder *builder, Scope &scope, Value *arr, ASTExpression &offset)
    {
        offset.accept(*this);
        auto index = cal_stack.top();
        cal_stack.pop();
        compulsiveTypeConvert(index, GET_INT32);

        // Examine wether the index is negative
        auto isIdxNeg = builder->create_icmp_lt(index, CONST_INT(0));
        auto negBB = newBasicBlock();
        auto posBB = newBasicBlock();
        builder->create_cond_br(isIdxNeg, negBB, posBB);
        builder->set_insert_point(negBB);
        auto negExcept = scope.find("neg_idx_except");
        builder->create_call(negExcept, {});
        builder->create_br(posBB);
        builder->set_insert_point(posBB);

        return getArrOrPtrAddr(arr, index);
    }
};
#endif
