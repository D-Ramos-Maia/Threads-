#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>

std::mutex mtx_global;

// Buffer de pedidos (tamanho 10)
int q_pedir[10]      = {0};
int q_lista[10]      = {0};
int q_item[10]       = {0};
int q_quantidade[10] = {0};

std::atomic<int>  var_vitoria(0);
std::atomic<bool> encerrado(false);

std::vector<std::string> lista = {
    "Acougue.txt",
    "Bebidas.txt",
    "Higiene_Pessoal.txt",
    "Padaria.txt",
    "Hortifruti.txt",
    "Laticinios_e_Congelados.txt",
    "Mercearia.txt"
};

std::vector<int> itens(lista.size());

// ---------------------------------------------------------------------------
// Conta quantas linhas (produtos) cada arquivo tem
// ---------------------------------------------------------------------------
void contarLinhas() {
    for (size_t i = 0; i < lista.size(); i++) {
        std::ifstream arquivo(lista[i]);
        std::string linha;
        int cont = 0;
        if (arquivo.is_open()) {
            while (std::getline(arquivo, linha)) cont++;
            itens[i] = cont;
        } else {
            itens[i] = 0;
            std::cerr << "Erro ao abrir: " << lista[i] << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// Adiciona quantidade aleatória (0-100) a cada produto nos arquivos
// ---------------------------------------------------------------------------
void quantida_mercadoria() {
    for (size_t i = 0; i < lista.size(); i++) {
        std::ifstream leitura(lista[i]);
        std::ofstream escrita("temp.txt");
        std::string linha;

        if (leitura.is_open() && escrita.is_open()) {
            while (std::getline(leitura, linha)) {
                int ale = rand() % 101;
                escrita << linha << " " << ale << "\n";
            }
            leitura.close();
            escrita.close();
            std::remove(lista[i].c_str());
            std::rename("temp.txt", lista[i].c_str());
        } else {
            std::cerr << "Erro ao processar: " << lista[i] << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// Lê o último número de uma linha (a quantidade em estoque)
// ---------------------------------------------------------------------------
int lerUltimoNumero(const std::string& linha) {
    std::stringstream ss(linha);
    std::string palavra;
    int numero = 0;
    while (ss >> palavra) {
        try   { numero = std::stoi(palavra); }
        catch (...) { continue; }
    }
    return numero;
}

// ---------------------------------------------------------------------------
// Retorna a quantidade em estoque de um produto específico
// ---------------------------------------------------------------------------
int item_ale(int ale_list, int linhaDesejada) {
    std::ifstream arquivo(lista[ale_list]);
    std::string linha;
    int linhaAtual = 0;

    if (arquivo.is_open()) {
        while (std::getline(arquivo, linha)) {
            if (linhaAtual == linhaDesejada) {
                arquivo.close();
                return lerUltimoNumero(linha);
            }
            linhaAtual++;
        }
        arquivo.close();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Grava o novo valor de estoque no arquivo correspondente
// ---------------------------------------------------------------------------
void gravar_estoque_atualizado(int listaIdx, int linhaIdx, int novoEstoque) {
    std::ifstream leitura(lista[listaIdx]);
    std::ofstream escrita("temp_update.txt");
    std::string linha;
    int linhaAtual = 0;

    if (leitura.is_open() && escrita.is_open()) {
        while (std::getline(leitura, linha)) {
            if (linhaAtual == linhaIdx) {
                size_t pos = linha.find_last_of(" ");
                std::string nomeProduto = linha.substr(0, pos);
                escrita << nomeProduto << " " << novoEstoque << "\n";
            } else {
                escrita << linha << "\n";
            }
            linhaAtual++;
        }
        leitura.close();
        escrita.close();
        std::remove(lista[listaIdx].c_str());
        std::rename("temp_update.txt", lista[listaIdx].c_str());
    }
}

// ---------------------------------------------------------------------------
// PRODUTOR — seleciona um produto com estoque e insere no buffer.
// ---------------------------------------------------------------------------
bool procurar_produtos(int solic) {
    int lis = -1, ite = -1, n = 0;

    while (!encerrado.load()) {
        std::lock_guard<std::mutex> trava(mtx_global);

        int q_lis = rand() % (int)lista.size();
        if (itens[q_lis] == 0) continue;
        int q_ite = rand() % itens[q_lis];
        int estoque_disponivel = item_ale(q_lis, q_ite);

        if (estoque_disponivel > 0) {
            lis = q_lis;
            ite = q_ite;
            n   = (rand() % estoque_disponivel) + 1;
            break;
        }
    }

    if (encerrado.load()) return false;

    bool registrado = false;
    while (!registrado && !encerrado.load()) {
        {
            std::lock_guard<std::mutex> trava(mtx_global);
            for (int contro = 0; contro < 10; contro++) {
                if (q_pedir[contro] == 0) {
                    q_lista[contro]      = lis;
                    q_item[contro]       = ite;
                    q_quantidade[contro] = n;
                    q_pedir[contro]      = solic;
                    std::cout << "[Produtor " << solic << "] Pedido inserido"
                              << " (vaga " << contro << "): " << n << " un.\n";
                    registrado = true;
                    break;
                }
            }
        }
        if (!registrado)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

// ---------------------------------------------------------------------------
// CONSUMIDOR — tenta pegar UMA vaga do buffer por chamada.
// O mutex garante que dois consumidores nunca processam a mesma vaga.
//   • estoque == 0  → limpa a vaga sem alterar var_vitoria
//   • pedido > estoque → vende apenas o que há disponível
// ---------------------------------------------------------------------------
void liberar_produto(int solic) {
    std::lock_guard<std::mutex> trava(mtx_global);

    for (int control = 0; control < 10; control++) {
        if (q_pedir[control] == 0) continue; // vaga vazia, passa

        int estoque_antigo = item_ale(q_lista[control], q_item[control]);

        if (estoque_antigo == 0) {
            std::cout << "[Consumidor " << solic << "] Pedido do Produtor "
                      << q_pedir[control] << " ignorado: produto sem estoque.\n";
        } else {
            int qtd_pedida  = q_quantidade[control];
            int qtd_vendida = (qtd_pedida <= estoque_antigo) ? qtd_pedida : estoque_antigo;
            int estoque_novo = estoque_antigo - qtd_vendida;

            gravar_estoque_atualizado(q_lista[control], q_item[control], estoque_novo);

            var_vitoria -= qtd_vendida;

            std::cout << "[Consumidor " << solic << "] Pedido do Produtor "
                      << q_pedir[control] << " processado: -" << qtd_vendida << " un."
                      << (qtd_vendida < qtd_pedida ? " (parcial)" : "")
                      << " | Restante: " << var_vitoria.load() << "\n";

            if (var_vitoria.load() <= 0)
                encerrado.store(true);
        }

        // Libera esta vaga e processa apenas ela por chamada
        q_pedir[control]      = 0;
        q_lista[control]      = 0;
        q_item[control]       = 0;
        q_quantidade[control] = 0;
        return; // <-- sai após processar uma vaga, evitando conflito entre consumidores
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    srand((unsigned)time(0));

    std::cout << "Aguardando carregamento do sistema...\n";
    contarLinhas();
    quantida_mercadoria();
    contarLinhas();

    // Calcula estoque inicial
    int total = 0;
    for (size_t i = 0; i < lista.size(); i++) {
        std::ifstream arquivo(lista[i]);
        std::string linha;
        while (std::getline(arquivo, linha))
            total += lerUltimoNumero(linha);
    }
    var_vitoria.store(total);
    std::cout << "Estoque inicial total: " << var_vitoria.load() << " itens.\n\n";

    // 3 threads PRODUTORAS
    std::vector<std::thread> produtores;
    for (int i = 0; i < 3; i++) {
        produtores.emplace_back([i]() {
            while (!encerrado.load()) {
                if (!procurar_produtos(i + 1)) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    // 3 threads CONSUMIDORAS
    std::vector<std::thread> consumidores;
    for (int i = 0; i < 3; i++) {
        consumidores.emplace_back([i]() {
            while (!encerrado.load()) {
                liberar_produto(i + 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
            }
        });
    }

    for (auto& t : produtores)
        if (t.joinable()) t.join();

    for (auto& t : consumidores)
        if (t.joinable()) t.join();

    std::cout << "\nSISTEMA ENCERRADO: Estoque Esgotado!\n";
    return 0;
}
