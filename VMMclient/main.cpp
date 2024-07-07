#include <iostream>
using namespace std;

class X {
public:
	X() {};
	~X() {};
	void func_X() {
		cout << "fxxk x\n" << endl;
	}
	virtual void say_my_name(){
		cout << "X\n" << endl;
	};
	static void look_at_me() {
		cout << "look_at_me" << endl;
	}
private:
	int x_a;
};

class XX :public X {
public:
	XX() :X() {

	}
	~XX();
	void say_my_name() override{
		cout << "XX\n" << endl;
	}
	virtual void this_is_XX_func() {
		cout << "this_is_XX_func\n" << endl;
	}
	void func_XX() {
		cout << "fxxk xx\n" << endl;
	}
private: 
	int xx_a;
	int xx_b;
};

int main() {
	X* x1= new XX();
	x1->say_my_name();
	X* x2 = new X();
	XX* xx1 = new XX();
 	int c = 0;
	cin >> c;
	return 0;
}