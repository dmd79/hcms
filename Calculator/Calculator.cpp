#include <stack>
#include <string>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <functional>
#include <vector>
#include <iostream>

// Informazioni operatore
struct OpInfo {
    int precedence;
    bool right_assoc;
};

std::map<std::string, OpInfo> operators = {
    {"+",   {1, false}},
    {"-",   {1, false}},
    {"*",   {2, false}},
    {"/",   {2, false}},
    // "%" non è un operatore binario: viene espanso contestualmente dal tokenizer
    {"NEG", {3, true}},   // meno unario
    {"^",   {4, true}}
};

// Funzioni supportate (trigonometriche in gradi)
std::map<std::string, std::function<double(double)>> functions = {
    // Trigonometriche (input in gradi)
    {"sin",   [](double x){ return sin(x*M_PI/180.0); }},
    {"cos",   [](double x){ return cos(x*M_PI/180.0); }},
    {"tan",   [](double x){
        // Gestisci casi speciali (90°, 270°, ecc.)
        double normalized = fmod(x, 360.0);
        if (normalized < 0) normalized += 360.0;
        if (fabs(normalized - 90.0) < 1e-10 || fabs(normalized - 270.0) < 1e-10) {
            return static_cast<double>(NAN); // tan(90°) = infinito
        }
        return tan(x*M_PI/180.0);
    }},
    {"asin",  [](double x){
        if (x < -1.0 || x > 1.0) return static_cast<double>(NAN);
        return asin(x)*180.0/M_PI;
    }},
    {"acos",  [](double x){
        if (x < -1.0 || x > 1.0) return static_cast<double>(NAN);
        return acos(x)*180.0/M_PI;
    }},
    {"atan",  [](double x){ return atan(x)*180.0/M_PI; }},

    // Iperboliche
    {"sinh",  [](double x){ return sinh(x); }},
    {"cosh",  [](double x){ return cosh(x); }},
    {"tanh",  [](double x){ return tanh(x); }},

    // Logaritmi ed esponenziali
    {"sqrt",  [](double x){
        if (x < 0) return static_cast<double>(NAN);
        return sqrt(x);
    }},
    {"cbrt",  [](double x){ return cbrt(x); }},  // radice cubica
    {"ln",    [](double x){
        if (x <= 0) return static_cast<double>(NAN);
        return log(x);
    }},
    {"log",   [](double x){
        if (x <= 0) return static_cast<double>(NAN);
        return log10(x);
    }},
    {"log2",  [](double x){
        if (x <= 0) return static_cast<double>(NAN);
        return log2(x);
    }},
    {"exp",   [](double x){ return exp(x); }},

    // Altre funzioni matematiche
    {"abs",   [](double x){ return fabs(x); }},
    {"ceil",  [](double x){ return ceil(x); }},
    {"floor", [](double x){ return floor(x); }},
    {"round", [](double x){ return round(x); }},

    // Funzioni avanzate
    {"factorial", std::function<double(double)>([](double x) {
        if(x < 0 || x != floor(x)) return static_cast<double>(NAN);
        if(x > 170) return static_cast<double>(INFINITY);
        double result = 1.0;
        for(int i = 2; i <= (int)x; i++) result *= static_cast<double>(i);
        return result;
    })},

    // Conversioni angolari
    {"deg",   [](double x){ return x*180.0/M_PI; }},  // rad -> deg
    {"rad",   [](double x){ return x*M_PI/180.0; }}   // deg -> rad
};

// Tokenizza l'espressione
std::vector<std::string> tokenize(const std::string &expr) {
    std::vector<std::string> tokens;
    size_t i = 0;

    while (i < expr.length()) {
        if (isspace(expr[i])) { i++; continue; }

        if (isdigit(expr[i]) || expr[i] == '.') {
            size_t j = i;
            while (j < expr.length() &&
                  (isdigit(expr[j]) || expr[j]=='.' ||
                   expr[j]=='e' || expr[j]=='E' ||
                   ((expr[j]=='+'||expr[j]=='-') &&
                    j>i && (expr[j-1]=='e'||expr[j-1]=='E'))))
                j++;
            tokens.push_back(expr.substr(i,j-i));
            i=j;
        }
        else if (isalpha(expr[i])) {
            size_t j=i;
            while (j<expr.length() && isalpha(expr[j])) j++;
            tokens.push_back(expr.substr(i,j-i));
            i=j;
        }
        else {
            tokens.push_back(std::string(1,expr[i]));
            i++;
        }
    }
    // Espansione contestuale di "%", stile calcolatrice fisica:
    //   a + b%  →  a + (a * b / 100)   → es. 200+10% = 220
    //   a - b%  →  a - (a * b / 100)   → es. 200-10% = 180
    //   a * b%  →  a * (b / 100)       → es. 100*10% = 10
    //   a / b%  →  a / (b / 100)       → es. 1000/10% = 10000
    //       b%  →  (b / 100)           → es. 50% = 0.5
    std::vector<std::string> out;
    for (size_t k = 0; k < tokens.size(); k++) {
        if (tokens[k] != "%") { out.push_back(tokens[k]); continue; }

        // Cerca l'operatore più vicino a sinistra al livello di parentesi 0
        int depth = 0;
        std::string ctx_op = "";
        std::string ctx_left = "";
        for (int j = (int)out.size()-1; j >= 0; j--) {
            if (out[j] == ")") depth++;
            else if (out[j] == "(") depth--;
            if (depth != 0) continue;
            if (out[j] == "+" || out[j] == "-") {
                ctx_op = out[j];
                // cerca il numero/gruppo immediatamente a sx dell'operatore
                for (int m = j-1; m >= 0; m--) {
                    if (out[m] == ")" || isdigit(out[m][0]) || out[m][0] == '.') {
                        ctx_left = out[m]; break;
                    }
                }
                break;
            }
            if (out[j] == "*" || out[j] == "/") { ctx_op = out[j]; break; }
        }

        // Trova l'indice di inizio dell'operando b (token prima del %)
        // per poterlo wrappare in parentesi se necessario
        auto find_operand_start = [&]() -> int {
            int d = 0;
            for (int j = (int)out.size()-1; j >= 0; j--) {
                if (out[j] == ")") d++;
                else if (out[j] == "(") d--;
                if (d == 0 && (out[j]=="*"||out[j]=="/"||out[j]=="+"||out[j]=="-")) return j+1;
                if (j == 0) return 0;
            }
            return 0;
        };

        if (ctx_op == "+" || ctx_op == "-") {
            // a ± b%  →  a ± b * ctx_left / 100
            out.push_back("*");
            out.push_back(ctx_left);
            out.push_back("/");
            out.push_back("100");
        } else {
            // * / o nessun contesto: b%  →  (b / 100)
            int start = find_operand_start();
            out.insert(out.begin() + start, "(");
            out.push_back("/");
            out.push_back("100");
            out.push_back(")");
        }
    }
    return out;
}

// Shunting-Yard + calcolo
double evalExpression(const std::string &expr) {
    auto tokens = tokenize(expr);
    std::stack<std::string> ops;
    std::stack<double> values;

    auto applyOp = [](double a, double b, const std::string &op) -> double {
        if(op=="+") return a+b;
        if(op=="-") return a-b;
        if(op=="*") return a*b;
        if(op=="/") return (b!=0)?a/b:NAN;
        if(op=="^") return pow(a,b);
        return NAN;
    };

    for (size_t i=0;i<tokens.size();i++) {
        const std::string &t = tokens[i];

        // numero
        if (isdigit(t[0]) || t[0]=='.') {
            values.push(atof(t.c_str()));
        }
        // funzione
        else if (functions.count(t)) {
            ops.push(t);
        }
        // parentesi aperta
        else if (t == "(") {
            ops.push(t);
        }
        // parentesi chiusa
        else if (t == ")") {
            while (!ops.empty() && ops.top()!="(") {
                std::string op = ops.top(); ops.pop();

                if(op=="NEG") {
                    double v = values.top(); values.pop();
                    values.push(-v);
                }
                else if(functions.count(op)) {
                    double v = values.top(); values.pop();
                    values.push(functions[op](v));
                }
                else {
                    double b=values.top(); values.pop();
                    double a=values.top(); values.pop();
                    values.push(applyOp(a,b,op));
                }
            }
            ops.pop(); // "("

            if(!ops.empty() && functions.count(ops.top())) {
                std::string f = ops.top(); ops.pop();
                double v = values.top(); values.pop();
                values.push(functions[f](v));
            }
        }
        // operatore
        else if (operators.count(t)) {

            // meno unario
            if (t=="-" &&
                (i==0 || tokens[i-1]=="(" || operators.count(tokens[i-1])))
            {
                ops.push("NEG");
                continue;
            }

            while (!ops.empty() && operators.count(ops.top())) {
                OpInfo a = operators[t];
                OpInfo b = operators[ops.top()];
                if ((a.right_assoc && a.precedence < b.precedence) ||
                    (!a.right_assoc && a.precedence <= b.precedence)) {
                    std::string op = ops.top(); ops.pop();

                    if(op=="NEG") {
                        double v = values.top(); values.pop();
                        values.push(-v);
                    } else {
                        double b=values.top(); values.pop();
                        double a=values.top(); values.pop();
                        values.push(applyOp(a,b,op));
                    }
                } else break;
            }
            ops.push(t);
        }
        else {
            return NAN;
        }
    }

    while(!ops.empty()) {
        std::string op = ops.top(); ops.pop();

        if(op=="NEG") {
            double v = values.top(); values.pop();
            values.push(-v);
        }
        else if(functions.count(op)) {
            double v = values.top(); values.pop();
            values.push(functions[op](v));
        }
        else {
            double b=values.top(); values.pop();
            double a=values.top(); values.pop();
            values.push(applyOp(a,b,op));
        }
    }

    return values.top();
}

// Verifica bilanciamento parentesi
bool parenthesesBalanced(const std::string &s) {
    int count = 0;
    for (char c : s) {
        if (c == '(') count++;
        else if (c == ')') {
            count--;
            if (count < 0) return false;
        }
    }
    return count == 0;
}