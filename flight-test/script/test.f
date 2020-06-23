func factorial(var value)
{
    if(value <= 1) { return 1; }
    return value * factorial(value - 1);
}

func func_ptr_test(var fp, var value)
{
    fp( "func_ptr_test => " + value );
}

func mul(var lhs, var rhs)
{
    print( "mul debug print => " + lhs * rhs);
    return lhs * rhs;
}

var global_var = 0;
func gv()
{
    global_var += 1;
}
  
func main() 
{
    // 再帰関数呼び出し
    print("\nfunction recursive------------------------------------------\n");
    print( "factorial => " + factorial(5) );


    // 関数ポインタ経由の関数呼び出し
    print("\nfunction ptr------------------------------------------------\n");
    var fp = print;
    func_ptr_test(print , 10);
    func_ptr_test(fp , 100);

    
    // for文
    print("\nfor---------------------------------------------------------\n");
    for(var i = 0 ; i < 10 ; i += 1)
    {
        print("for counter i =>" + i);
        if( i == 5 ) { break; }
    }
    
    // 関数
    print("\nfunction----------------------------------------------------\n");
    var ret = mul(mul(10,10),20);
    print("mul return value => " + ret);

    
    // if文
    print("\nif---------------------------------------------------------\n");
    var if_var = 5;
    print("if_var => " + if_var);
    if( if_var == 1 ) { print("if_var == 1"); }
    else if( if_var == 2) { print("if_var == 2"); }
    else if( if_var == 3) { print("if_var == 3"); }
    else { print("if_var else"); }

    
    // while文
    print("\nwhile-----------------------------------------------------\n");
    var while_cond = 0;
    while( while_cond < 5 ) { while_cond += 1; }
    print("while cond => " + while_cond);

    
    // 複数の変数宣言
    print("\nmulti var-------------------------------------------------\n");
    var var_1 = ( 3 + 4 ) * 2 , var_2 = ( 2 * 4 + factorial(3)) + 3 * 4;
    print( "var_1 => " + var_1);
    print( "var_2 => " + var_2);

    // グローバル変数の操作
    print("\nglobal var-------------------------------------------------\n");
    gv();
    gv();
    print( "global_var => " + global_var );

    // ホスト関数の呼び出し
    print( "call host function => " + add( 1 , 2 ) );

    return 0;
}

return main();
