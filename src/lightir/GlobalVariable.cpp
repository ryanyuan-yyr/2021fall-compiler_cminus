//
// Created by cqy on 2020/6/29.
//
#include "GlobalVariable.h"
#include "IRprinter.h"

GlobalVariable::GlobalVariable(std::string name, Module *m, Type *ty, bool is_const, Constant *init)
    : User(ty, name, init != nullptr), is_const_(is_const), init_val_(init)
{
    m->add_global_variable(this);
    if (init)
    {
        this->set_operand(0, init);
    }
} //global操作数为initval

GlobalVariable *GlobalVariable::create(std::string name, Module *m, Type *ty, bool is_const,
                                       Constant *init = nullptr)
{
    return new GlobalVariable(name, m, PointerType::get(ty), is_const, init);
}

std::string GlobalVariable::print()
{
    std::string global_val_ir;
    global_val_ir += print_as_op(this, false);
    global_val_ir += " = ";
    global_val_ir += (this->is_const() ? "constant " : "global ");
    global_val_ir += this->get_print_type()->get_pointer_element_type()->print();
    if (get_init())
    {
        global_val_ir += " ";
        global_val_ir += this->get_init()->print();
    }
    else
        global_val_ir += "zeroinitializer";
    return global_val_ir;
}