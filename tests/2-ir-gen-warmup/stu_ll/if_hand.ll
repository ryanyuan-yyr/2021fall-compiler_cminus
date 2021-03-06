define dso_local i32 @main() #0{
    %a_ptr = alloca float
    
    ; 5.555 cannot be encoded into binary float precisely
    ; To see how I convert 5.555 roughly to float, refer to my report, “实验难点” part. 
    store float 0x40163851E0000000, float* %a_ptr
    %a_val = load float, float* %a_ptr

    ; In source file, `1` is an int. It should be converted to floated
    ; before being calculated. 
    %fp1 = sitofp i32 1 to float
    
    %comp_res = fcmp ugt float %a_val, %fp1

    br i1 %comp_res, label %if_true, label %if_false

if_true:
    ret i32 233

if_false:
    ret i32 0
}