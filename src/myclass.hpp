namespace jim {
template <typename T>
class MyClass {
public:
    MyClass () : value_() {}
    T value_;
};

class Baseclass {
public:
    Baseclass () : value0_(5) {}
    int value0_;
};

template <typename T>
class Subclass1 : public Baseclass {
public:
    Subclass1 () : Baseclass(), value1_(0) {}
    T value1_;
};

template <typename T>
class Subclass2 : public Baseclass {
public:
    Subclass2 () : Baseclass(), value2_(10) {}
    T value2_;
};

}

template <typename T1, typename T2>
T1 add (const T1 &a, const T2 &b) {
    return a + b;
}
