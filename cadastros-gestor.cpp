// ============================================================
//  Sistema de Cadastro de Clientes - C++  (v3.0)
//  Compilar: g++ cadastros-gestor.cpp -o cadastro -std=c++17
// ============================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// ------------------- cores ANSI -------------------
#define RESET    "\033[0m"
#define NEGRITO  "\033[1m"
#define VERDE    "\033[32m"
#define VERMELHO "\033[31m"
#define AMARELO  "\033[33m"
#define CIANO    "\033[36m"

const char* ARQUIVO = "clientes.dat";
const char* BACKUP  = "clientes.bak";
const char* TEMPFIL = "clientes.tmp";
const char* SEQFILE = "clientes.seq";
const char* CSVFILE = "clientes.csv";

// ------------------- estrutura -------------------
struct Cliente {
    int32_t id;
    char    nome[60];
    char    email[80];
    char    telefone[20];
    int32_t idade;
    bool    ativo;
    char    dataCadastro[20];
};
static_assert(sizeof(Cliente) == 192, "Layout inesperado de Cliente");

// ------------------- console -------------------
void prepararConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &modo))
        SetConsoleMode(h, modo | 0x0004);
#endif
}

// ------------------- texto UTF-8 -------------------
size_t tamUtf8(const string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
    return n;
}

string pad(const string& s, size_t larg) {
    string r;
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = s[i];
        size_t len = (c < 0x80) ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        if (n + 1 > larg) break;
        r.append(s, i, len);
        i += len; n++;
    }
    if (tamUtf8(s) > larg && larg > 1) {
        r = pad(r, larg - 1) + "~";
        return r;
    }
    return r + string(larg - n, ' ');
}

string paraMinusculo(const string& s) {
    string r(s);
    transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return (char)tolower(c); });
    return r;
}

bool contem(const string& texto, const string& busca) {
    return paraMinusculo(texto).find(paraMinusculo(busca)) != string::npos;
}

string aparar(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ------------------- utilitários de entrada -------------------
void limparBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void checarEOF() {
    if (cin.eof()) {
        cout << "\n" << AMARELO << "  Entrada encerrada. Saindo..." << RESET << "\n";
        exit(0);
    }
}

void pausar() {
    cout << CIANO << "\n  Pressione ENTER para continuar..." << RESET;
    cin.clear();
    cin.get();
}

void copiarTexto(char* dst, int tam, const string& src) {
    memset(dst, 0, tam);
    strncpy(dst, src.c_str(), tam - 1);
}

bool lerTexto(const string& rotulo, string& out, size_t maxBytes) {
    cout << rotulo;
    string linha;
    if (!getline(cin, linha)) { checarEOF(); cin.clear(); return false; }
    linha = aparar(linha);
    if (linha.size() >= maxBytes) {
        cout << VERMELHO << "  [!] Texto muito longo (maximo " << (maxBytes - 1) << " bytes).\n" << RESET;
        return false;
    }
    out = linha;
    return true;
}

int lerInteiro(const string& rotulo, int min, int max) {
    while (true) {
        cout << rotulo;
        string linha;
        if (!getline(cin, linha)) { checarEOF(); cin.clear(); continue; }
        linha = aparar(linha);
        bool ok = !linha.empty() && linha.size() < 10;
        for (size_t i = 0; ok && i < linha.size(); i++)
            if (!isdigit((unsigned char)linha[i]) && !(i == 0 && linha[i] == '-' && linha.size() > 1)) ok = false;
        if (!ok) { cout << VERMELHO << "  [!] Entrada invalida.\n" << RESET; continue; }
        int v = atoi(linha.c_str());
        if (v >= min && v <= max) return v;
        cout << VERMELHO << "  [!] Digite um valor entre " << min << " e " << max << ".\n" << RESET;
    }
}

// ------------------- validações -------------------
bool emailValido(const string& e) {
    if (e.size() < 5 || e.find(' ') != string::npos || e.find(';') != string::npos) return false;
    size_t at = e.find('@');
    if (at == string::npos || at == 0 || e.find('@', at + 1) != string::npos) return false;
    string dom = e.substr(at + 1);
    size_t ponto = dom.find('.');
    if (ponto == string::npos || ponto == 0 || dom.back() == '.') return false;
    return dom.find("..") == string::npos;
}

bool telefoneValido(const string& t) {
    int digitos = 0;
    for (unsigned char c : t) {
        if (isdigit(c)) digitos++;
        else if (!strchr(" ()-+", c)) return false;
    }
    return digitos >= 8 && digitos <= 13;
}

bool nomeValido(const string& n) { return n.size() >= 2 && n.find(';') == string::npos; }

// ------------------- data -------------------
void dataAtual(char* buf, int tam) {
    time_t t = time(nullptr);
    tm* now = localtime(&t);
    snprintf(buf, tam, "%02d/%02d/%04d", now->tm_mday, now->tm_mon + 1, now->tm_year + 1900);
}

// ------------------- banner -------------------
void banner() {
    cout << "\n" << CIANO;
    cout << "  ██████╗ █████╗ ██████╗  █████╗ ███████╗████████╗██████╗  ██████╗\n";
    cout << " ██╔════╝██╔══██╗██╔══██╗██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗\n";
    cout << " ██║     ███████║██║  ██║███████║███████╗   ██║   ██████╔╝██║   ██║\n";
    cout << " ██║     ██╔══██║██║  ██║██╔══██║╚════██║   ██║   ██╔══██╗██║   ██║\n";
    cout << " ╚██████╗██║  ██║██████╔╝██║  ██║███████║   ██║   ██╔══██╗██║   ██║\n";
    cout << "  ╚═════╝╚═╝  ╚═╝╚═════╝ ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝\n";
    cout << RESET;
    cout << "  ===============================================================\n";
    cout << NEGRITO << "      Sistema de Cadastro de Clientes  |  v3.0  |  C++\n" << RESET;
    cout << "  ===============================================================\n\n";
}

// ------------------- persistência -------------------
vector<Cliente> carregarTodos() {
    vector<Cliente> v;
    ifstream in(ARQUIVO, ios::binary);
    if (!in) return v;
    Cliente c;
    while (in.read(reinterpret_cast<char*>(&c), sizeof(Cliente))) v.push_back(c);
    return v;
}

vector<Cliente> carregarAtivos() {
    vector<Cliente> v;
    for (const auto& c : carregarTodos()) if (c.ativo) v.push_back(c);
    return v;
}

bool copiarArquivo(const char* de, const char* para) {
    ifstream in(de, ios::binary);
    if (!in) return false;
    ofstream out(para, ios::binary | ios::trunc);
    if (!out) return false;
    out << in.rdbuf();
    return (bool)out;
}

void fazerBackup() { copiarArquivo(ARQUIVO, BACKUP); }

bool salvarTodos(const vector<Cliente>& v) {
    {
        ofstream out(TEMPFIL, ios::binary | ios::trunc);
        if (!out) return false;
        for (const auto& c : v) out.write(reinterpret_cast<const char*>(&c), sizeof(Cliente));
        out.flush();
        if (!out) return false;
    }
    remove(ARQUIVO);
    return rename(TEMPFIL, ARQUIVO) == 0;
}

int gerarId() {
    int proximo = 1;
    { ifstream s(SEQFILE); if (s) s >> proximo; if (proximo < 1) proximo = 1; }
    for (const auto& c : carregarTodos()) if (c.id >= proximo) proximo = c.id + 1;
    ofstream s(SEQFILE, ios::trunc);
    if (s) s << (proximo + 1);
    return proximo;
}

bool emailExiste(const vector<Cliente>& v, const string& email, int ignorarId = -1) {
    string alvo = paraMinusculo(email);
    for (const auto& c : v)
        if (c.ativo && c.id != ignorarId && paraMinusculo(c.email) == alvo) return true;
    return false;
}

// ------------------- entradas validadas -------------------
bool pedirNome(const string& rot, char* dst, bool editando) {
    while (true) {
        string s;
        if (!lerTexto(rot, s, 60)) continue;
        if (s.empty()) {
            if (editando) return false;
            cout << VERMELHO << "  [!] Campo obrigatorio.\n" << RESET; continue;
        }
        if (!nomeValido(s)) { cout << VERMELHO << "  [!] Nome invalido (min. 2 letras, sem ';').\n" << RESET; continue; }
        copiarTexto(dst, 60, s); return true;
    }
}

bool pedirEmail(const string& rot, char* dst, const vector<Cliente>& todos, int meuId, bool editando) {
    while (true) {
        string s;
        if (!lerTexto(rot, s, 80)) continue;
        if (s.empty()) {
            if (editando) return false;
            cout << VERMELHO << "  [!] Campo obrigatorio.\n" << RESET; continue;
        }
        if (!emailValido(s)) { cout << VERMELHO << "  [!] E-mail invalido (ex.: nome@dominio.com).\n" << RESET; continue; }
        if (emailExiste(todos, s, meuId)) { cout << VERMELHO << "  [!] Ja existe um cliente com esse e-mail.\n" << RESET; continue; }
        copiarTexto(dst, 80, s); return true;
    }
}

bool pedirTelefone(const string& rot, char* dst, bool editando) {
    while (true) {
        string s;
        if (!lerTexto(rot, s, 20)) continue;
        if (s.empty()) {
            if (editando) return false;
            cout << VERMELHO << "  [!] Campo obrigatorio.\n" << RESET; continue;
        }
        if (!telefoneValido(s)) { cout << VERMELHO << "  [!] Telefone invalido (8 a 13 digitos; aceita + ( ) - e espaco).\n" << RESET; continue; }
        copiarTexto(dst, 20, s); return true;
    }
}

// ------------------- cadastrar -------------------
void cadastrar() {
    banner();
    cout << NEGRITO << "  == NOVO CADASTRO ==\n\n" << RESET;
    auto todos = carregarTodos();
    Cliente c; memset(&c, 0, sizeof(c));
    c.ativo = true;
    dataAtual(c.dataCadastro, sizeof(c.dataCadastro));
    pedirNome("  Nome.....: ", c.nome, false);
    pedirEmail("  Email....: ", c.email, todos, -1, false);
    pedirTelefone("  Telefone.: ", c.telefone, false);
    c.idade = lerInteiro("  Idade....: ", 0, 130);
    c.id = gerarId();
    todos.push_back(c);
    if (!salvarTodos(todos)) {
        cout << VERMELHO << "  [ERRO] Nao foi possivel gravar o arquivo.\n" << RESET;
        pausar(); return;
    }
    cout << VERDE << "\n  [OK] Cliente cadastrado com sucesso! ID: " << c.id
         << "  Data: " << c.dataCadastro << "\n" << RESET;
    pausar();
}

// ------------------- tabela -------------------
void cabecalhoTabela(bool comData) {
    cout << "  " << NEGRITO << pad("ID", 6) << pad("NOME", 25) << pad("EMAIL", 30)
         << pad("TELEFONE", 16) << pad("IDADE", 6) << (comData ? "CADASTRO" : "") << RESET << "\n";
    cout << "  " << string(comData ? 93 : 83, '-') << "\n";
}

void linhaTabela(const Cliente& c, bool comData) {
    cout << "  " << pad(to_string(c.id), 6) << pad(c.nome, 25) << pad(c.email, 30)
         << pad(c.telefone, 16) << pad(to_string(c.idade), 6) << (comData ? c.dataCadastro : "") << "\n";
}

// ------------------- listar -------------------
void listar() {
    auto v = carregarAtivos();
    banner();
    if (v.empty()) {
        cout << AMARELO << "  Nenhum cliente cadastrado.\n" << RESET;
        pausar(); return;
    }
    cout << NEGRITO << "  == LISTAR CLIENTES ==\n\n" << RESET;
    cout << "  Ordenar por:\n  1 - ID (padrao)\n  2 - Nome (A-Z)\n  3 - Idade (crescente)\n";
    int ord = lerInteiro("\n  Opcao: ", 1, 3);

    switch (ord) {
        case 1: sort(v.begin(), v.end(), [](const Cliente& a, const Cliente& b){ return a.id < b.id; }); break;
        case 2: sort(v.begin(), v.end(), [](const Cliente& a, const Cliente& b){ return paraMinusculo(a.nome) < paraMinusculo(b.nome); }); break;
        case 3: sort(v.begin(), v.end(), [](const Cliente& a, const Cliente& b){ return a.idade != b.idade ? a.idade < b.idade : a.id < b.id; }); break;
    }

    const int PP = 10;
    int total = (int)v.size();
    int paginas = (total + PP - 1) / PP;
    int pag = 0;
    while (true) {
        banner();
        cout << NEGRITO << "  == CLIENTES (pagina " << (pag + 1) << "/" << paginas << ") ==\n\n" << RESET;
        cabecalhoTabela(true);
        int ini = pag * PP, fim = min(ini + PP, total);
        for (int i = ini; i < fim; i++) linhaTabela(v[i], true);
        cout << "\n  Total: " << VERDE << total << RESET << " cliente(s).\n";
        if (paginas == 1) break;
        cout << "\n  [1] Proxima  [2] Anterior  [3] Primeira  [4] Ultima  [0] Voltar\n";
        int op = lerInteiro("  Opcao: ", 0, 4);
        if (op == 0) break;
        if (op == 1 && pag < paginas - 1) pag++;
        if (op == 2 && pag > 0) pag--;
        if (op == 3) pag = 0;
        if (op == 4) pag = paginas - 1;
    }
    pausar();
}

// ------------------- buscar por ID -------------------
void buscarPorId() {
    banner();
    cout << NEGRITO << "  == BUSCAR CLIENTE POR ID ==\n\n" << RESET;
    int id = lerInteiro("  ID do cliente: ", 1, 999999);
    for (const auto& c : carregarAtivos()) {
        if (c.id == id) {
            cout << "\n" << VERDE;
            cout << "  +----------------------------------------------+\n";
            cout << "  | ID.......: " << pad(to_string(c.id), 33) << "|\n";
            cout << "  | Nome.....: " << pad(c.nome, 33) << "|\n";
            cout << "  | Email....: " << pad(c.email, 33) << "|\n";
            cout << "  | Telefone.: " << pad(c.telefone, 33) << "|\n";
            cout << "  | Idade....: " << pad(to_string(c.idade), 33) << "|\n";
            cout << "  | Cadastro.: " << pad(c.dataCadastro, 33) << "|\n";
            cout << "  +----------------------------------------------+\n" << RESET;
            pausar(); return;
        }
    }
    cout << VERMELHO << "\n  [!] Cliente nao encontrado.\n" << RESET;
    pausar();
}

// ------------------- buscar por texto -------------------
void buscarPorTexto() {
    banner();
    cout << NEGRITO << "  == BUSCAR CLIENTE ==\n\n" << RESET;
    cout << "  Buscar em:\n  1 - Nome\n  2 - E-mail\n  3 - Telefone\n";
    int campo = lerInteiro("\n  Opcao: ", 1, 3);
    string termo;
    while (true) {
        if (!lerTexto("  Digite parte do texto: ", termo, 60)) continue;
        if (!termo.empty()) break;
        cout << VERMELHO << "  [!] Campo obrigatorio.\n" << RESET;
    }
    vector<Cliente> achados;
    for (const auto& c : carregarAtivos()) {
        const char* alvo = campo == 1 ? c.nome : campo == 2 ? c.email : c.telefone;
        if (contem(alvo, termo)) achados.push_back(c);
    }
    if (achados.empty()) {
        cout << VERMELHO << "\n  [!] Nenhum cliente encontrado com '" << termo << "'.\n" << RESET;
        pausar(); return;
    }
    cout << "\n  " << VERDE << achados.size() << " resultado(s):\n\n" << RESET;
    cabecalhoTabela(false);
    for (const auto& c : achados) linhaTabela(c, false);
    pausar();
}

// ------------------- editar -------------------
void editar() {
    banner();
    cout << NEGRITO << "  == EDITAR CLIENTE ==\n\n" << RESET;
    int id = lerInteiro("  ID do cliente: ", 1, 999999);
    auto todos = carregarTodos();
    Cliente* alvo = nullptr;
    for (auto& c : todos) if (c.id == id && c.ativo) { alvo = &c; break; }
    if (!alvo) {
        cout << VERMELHO << "  [!] Cliente nao encontrado.\n" << RESET;
        pausar(); return;
    }
    Cliente c = *alvo;
    cout << "\n  Dados atuais: " << NEGRITO << c.nome << RESET << " | " << c.email << " | "
         << c.telefone << " | " << c.idade << " anos | cadastrado em " << c.dataCadastro << "\n";
    cout << "  (ENTER em branco mantem o valor atual)\n\n";
    pedirNome(string("  Novo nome [") + c.nome + "]: ", c.nome, true);
    pedirEmail(string("  Novo email [") + c.email + "]: ", c.email, todos, c.id, true);
    pedirTelefone(string("  Novo telefone [") + c.telefone + "]: ", c.telefone, true);
    while (true) {
        string s;
        if (!lerTexto("  Nova idade [" + to_string(c.idade) + "]: ", s, 10)) continue;
        if (s.empty()) break;
        bool num = all_of(s.begin(), s.end(), [](unsigned char ch){ return isdigit(ch); });
        int v = num ? atoi(s.c_str()) : -1;
        if (v >= 0 && v <= 130) { c.idade = v; break; }
        cout << VERMELHO << "  [!] Idade invalida (0 a 130).\n" << RESET;
    }
    fazerBackup();
    *alvo = c;
    if (!salvarTodos(todos)) {
        cout << VERMELHO << "  [ERRO] Falha ao gravar. Backup em '" << BACKUP << "'.\n" << RESET;
        pausar(); return;
    }
    cout << VERDE << "\n  [OK] Cliente atualizado!\n" << RESET;
    pausar();
}

// ------------------- remover -------------------
void remover() {
    banner();
    cout << NEGRITO << "  == REMOVER CLIENTE ==\n\n" << RESET;
    int id = lerInteiro("  ID do cliente: ", 1, 999999);
    auto todos = carregarTodos();
    Cliente* alvo = nullptr;
    for (auto& c : todos) if (c.id == id && c.ativo) { alvo = &c; break; }
    if (!alvo) {
        cout << VERMELHO << "  [!] Cliente nao encontrado.\n" << RESET;
        pausar(); return;
    }
    cout << "\n  Cliente: " << NEGRITO << alvo->nome << RESET << " (" << alvo->email << ")\n";
    if (lerInteiro("  Confirmar remocao? (1=Sim / 0=Nao): ", 0, 1) == 0) {
        cout << AMARELO << "  Operacao cancelada.\n" << RESET;
        pausar(); return;
    }
    fazerBackup();
    alvo->ativo = false;
    if (!salvarTodos(todos)) {
        cout << VERMELHO << "  [ERRO] Falha ao gravar. Backup em '" << BACKUP << "'.\n" << RESET;
        pausar(); return;
    }
    cout << VERDE << "\n  [OK] Cliente removido!\n" << RESET;
    pausar();
}

// ------------------- estatísticas -------------------
void linhaEst(const string& rot, const string& val) {
    cout << "  | " << pad(rot, 24) << ": " << pad(val, 25) << "|\n";
}

void estatisticas() {
    banner();
    cout << NEGRITO << "  == ESTATISTICAS ==\n\n" << RESET;
    auto v = carregarAtivos();
    if (v.empty()) {
        cout << AMARELO << "  Nenhum cliente cadastrado.\n" << RESET;
        pausar(); return;
    }
    int total = (int)v.size();
    long long soma = 0;
    int idxVelho = 0, idxNovo = 0, menores18 = 0, maiores60 = 0;
    for (int i = 0; i < total; i++) {
        soma += v[i].idade;
        if (v[i].idade > v[idxVelho].idade) idxVelho = i;
        if (v[i].idade < v[idxNovo].idade) idxNovo = i;
        if (v[i].idade < 18) menores18++;
        if (v[i].idade >= 60) maiores60++;
    }
    ostringstream media;
    media << fixed << setprecision(1) << ((double)soma / total) << " anos";
    string bar = "  +" + string(53, '-') + "+\n";
    cout << bar;
    linhaEst("Clientes ativos", to_string(total));
    linhaEst("Idade media", media.str());
    linhaEst("Cliente mais velho", string(v[idxVelho].nome) + " (" + to_string(v[idxVelho].idade) + ")");
    linhaEst("Cliente mais novo", string(v[idxNovo].nome) + " (" + to_string(v[idxNovo].idade) + ")");
    linhaEst("Menores de 18", to_string(menores18));
    linhaEst("Com 60 anos ou mais", to_string(maiores60));
    cout << bar;
    pausar();
}

// ------------------- CSV -------------------
string csvEscapar(const string& s) {
    if (s.find_first_of(";\"\n\r") == string::npos) return s;
    string r = "\"";
    for (char ch : s) { if (ch == '"') r += '"'; r += ch; }
    return r + "\"";
}

vector<string> csvDividir(const string& linha) {
    vector<string> campos;
    string atual;
    bool aspas = false;
    for (size_t i = 0; i < linha.size(); i++) {
        char ch = linha[i];
        if (aspas) {
            if (ch == '"') {
                if (i + 1 < linha.size() && linha[i + 1] == '"') { atual += '"'; i++; }
                else aspas = false;
            } else atual += ch;
        } else if (ch == '"') aspas = true;
        else if (ch == ';') { campos.push_back(atual); atual.clear(); }
        else atual += ch;
    }
    campos.push_back(atual);
    return campos;
}

void exportarCSV() {
    banner();
    cout << NEGRITO << "  == EXPORTAR PARA CSV ==\n\n" << RESET;
    auto v = carregarAtivos();
    if (v.empty()) {
        cout << AMARELO << "  Nenhum cliente cadastrado.\n" << RESET;
        pausar(); return;
    }
    ofstream out(CSVFILE, ios::trunc);
    if (!out) {
        cout << VERMELHO << "  [ERRO] Nao foi possivel criar o arquivo CSV.\n" << RESET;
        pausar(); return;
    }
    out << "\xEF\xBB\xBF";
    out << "ID;Nome;Email;Telefone;Idade;DataCadastro\n";
    for (const auto& c : v)
        out << c.id << ";" << csvEscapar(c.nome) << ";" << csvEscapar(c.email) << ";"
            << csvEscapar(c.telefone) << ";" << c.idade << ";" << c.dataCadastro << "\n";
    out.close();
    cout << VERDE << "  [OK] " << v.size() << " cliente(s) exportado(s) para '" << CSVFILE << "'.\n" << RESET;
    pausar();
}

void importarCSV() {
    banner();
    cout << NEGRITO << "  == IMPORTAR CSV ==\n\n" << RESET;
    string nomeArq;
    cout << "  Arquivo (ENTER = " << CSVFILE << "): ";
    getline(cin, nomeArq);
    nomeArq = aparar(nomeArq);
    if (nomeArq.empty()) nomeArq = CSVFILE;
    ifstream in(nomeArq);
    if (!in) {
        cout << VERMELHO << "  [ERRO] Nao foi possivel abrir '" << nomeArq << "'.\n" << RESET;
        pausar(); return;
    }
    auto todos = carregarTodos();
    int importados = 0, ignorados = 0;
    string linha;
    bool primeira = true;
    while (getline(in, linha)) {
        if (primeira) {
            primeira = false;
            if (linha.compare(0, 3, "\xEF\xBB\xBF") == 0) linha.erase(0, 3);
            continue;
        }
        linha = aparar(linha);
        if (linha.empty()) continue;
        auto f = csvDividir(linha);
        if (f.size() < 5) { ignorados++; continue; }
        string nome = aparar(f[1]), email = aparar(f[2]), tel = aparar(f[3]), idadeS = aparar(f[4]);
        bool numIdade = !idadeS.empty() && idadeS.size() < 4 && all_of(idadeS.begin(), idadeS.end(), [](unsigned char ch){ return isdigit(ch); });
        int idade = numIdade ? atoi(idadeS.c_str()) : -1;
        if (!nomeValido(nome) || nome.size() >= 60 || !emailValido(email) || email.size() >= 80 ||
            !telefoneValido(tel) || tel.size() >= 20 || idade < 0 || idade > 130 || emailExiste(todos, email)) {
            ignorados++; continue;
        }
        Cliente c; memset(&c, 0, sizeof(c));
        c.ativo = true;
        copiarTexto(c.nome, sizeof(c.nome), nome);
        copiarTexto(c.email, sizeof(c.email), email);
        copiarTexto(c.telefone, sizeof(c.telefone), tel);
        c.idade = idade;
        string data = f.size() > 5 ? aparar(f[5]) : "";
        if (data.size() == 10 && data[2] == '/' && data[5] == '/') copiarTexto(c.dataCadastro, sizeof(c.dataCadastro), data);
        else dataAtual(c.dataCadastro, sizeof(c.dataCadastro));
        c.id = gerarId();
        todos.push_back(c);
        importados++;
    }
    in.close();
    if (importados > 0) {
        fazerBackup();
        if (!salvarTodos(todos)) {
            cout << VERMELHO << "  [ERRO] Falha ao gravar.\n" << RESET;
            pausar(); return;
        }
    }
    cout << VERDE << "  [OK] " << importados << " importado(s)." << RESET
         << AMARELO << "  " << ignorados << " ignorado(s) (invalidos ou e-mail duplicado).\n" << RESET;
    pausar();
}

// ------------------- compactar base -------------------
void compactar() {
    banner();
    cout << NEGRITO << "  == COMPACTAR BASE ==\n\n" << RESET;
    auto todos = carregarTodos();
    auto ativos = carregarAtivos();
    int removidos = (int)(todos.size() - ativos.size());
    if (removidos == 0) {
        cout << AMARELO << "  Nada para compactar: nao ha registros removidos.\n" << RESET;
        pausar(); return;
    }
    cout << "  Registros removidos que serao apagados em definitivo: " << removidos << "\n";
    if (lerInteiro("  Confirmar? (1=Sim / 0=Nao): ", 0, 1) == 0) {
        cout << AMARELO << "  Operacao cancelada.\n" << RESET;
        pausar(); return;
    }
    fazerBackup();
    if (!salvarTodos(ativos)) {
        cout << VERMELHO << "  [ERRO] Falha ao gravar.\n" << RESET;
        pausar(); return;
    }
    cout << VERDE << "  [OK] Base compactada. " << removidos << " registro(s) eliminado(s). "
         << "Backup em '" << BACKUP << "'.\n" << RESET;
    pausar();
}

// ------------------- main -------------------
void itemMenu(const char* n, const string& txt) {
    cout << "  |  " << NEGRITO << pad(n, 2) << RESET << " - " << pad(txt, 35) << "|\n";
}

int main() {
    prepararConsole();
    int opcao;
    do {
        banner();
        cout << "  +" << string(42, '-') << "+\n";
        itemMenu("1", "Cadastrar cliente");
        itemMenu("2", "Listar clientes (ordenar/paginar)");
        itemMenu("3", "Buscar cliente por ID");
        itemMenu("4", "Buscar por nome/e-mail/telefone");
        itemMenu("5", "Editar cliente");
        itemMenu("6", "Remover cliente");
        itemMenu("7", "Estatisticas");
        itemMenu("8", "Exportar para CSV");
        itemMenu("9", "Importar de CSV");
        itemMenu("10", "Compactar base (apagar removidos)");
        itemMenu("0", "Sair");
        cout << "  +" << string(42, '-') << "+\n";

        opcao = lerInteiro("\n  Opcao: ", 0, 10);
        switch (opcao) {
            case 1: cadastrar(); break;
            case 2: listar(); break;
            case 3: buscarPorId(); break;
            case 4: buscarPorTexto(); break;
            case 5: editar(); break;
            case 6: remover(); break;
            case 7: estatisticas(); break;
            case 8: exportarCSV(); break;
            case 9: importarCSV(); break;
            case 10: compactar(); break;
            case 0: cout << "\n" << VERDE << "  Ate logo!\n\n" << RESET; break;
        }
    } while (opcao != 0);
    return 0;
}
