y este?
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <map>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>

using namespace std;

struct Node {
    string id;
    double x, y, demand, e_i, l_i, s_i;
    bool is_depot;
    
    Node(string _id, double _x, double _y, double _d, double _e, double _l, double _s, bool _is_depot)
        : id(_id), x(_x), y(_y), demand(_d), e_i(_e), l_i(_l), s_i(_s), is_depot(_is_depot) {}
};

struct Penalty {
    string id;
    double llegada;
    double e_i, l_i;
    double tardanza;
    double exceso;
    bool is_capacity;
};

struct EvalResult {
    double dist;
    double tiempo;
    double carga;
    double tardanza_total;  // FIX 2: suma de magnitudes de tardanza
    double exceso_carga;    // FIX 2: exceso de capacidad total
    vector<Penalty> penalizaciones;
};

typedef map<string, vector<vector<Node*>>> Solucion;

double calcular_distancia(const Node* n1, const Node* n2) {
    return std::hypot(n1->x - n2->x, n1->y - n2->y);
}

EvalResult evaluar_ruta(const vector<Node*>& ruta, const Node* deposito, double capacidad, double v = 1.0) {
    EvalResult res = {0.0, 0.0, 0.0, 0.0, 0.0, {}};
    const Node* nodo_actual = deposito;
    
    for (const Node* cliente : ruta) {
        double dist = calcular_distancia(nodo_actual, cliente);
        res.dist += dist;
        double tiempo_viaje = dist / v;
        double llegada = res.tiempo + tiempo_viaje;
        
        if (llegada < cliente->e_i) {
            res.tiempo = cliente->e_i;
        } else if (llegada > cliente->l_i) {
            double tardanza = llegada - cliente->l_i;
            res.tardanza_total += tardanza;  // FIX 2: acumular magnitud
            Penalty p = {cliente->id, llegada, cliente->e_i, cliente->l_i, tardanza, 0.0, false};
            res.penalizaciones.push_back(p);
            res.tiempo = llegada;
        } else {
            res.tiempo = llegada;
        }
        
        res.tiempo += cliente->s_i;
        res.carga += cliente->demand;
        nodo_actual = cliente;
    }
    
    double dist_retorno = calcular_distancia(nodo_actual, deposito);
    res.dist += dist_retorno;
    res.tiempo += (dist_retorno / v);
    
    if (res.carga > capacidad) {
        res.exceso_carga = res.carga - capacidad;  // FIX 2: acumular magnitud
        Penalty p = {"CAPACIDAD", 0.0, 0.0, 0.0, 0.0, res.exceso_carga, true};
        res.penalizaciones.push_back(p);
    }
    
    return res;
}

// FIX 2: FO proporcional a la magnitud del error, no al numero de violaciones
double calcular_fo_ruta(const EvalResult& eval) {
    return eval.dist + eval.tardanza_total * 100.0 + eval.exceso_carga * 500.0;
}

double calcular_fo_total(const Solucion& solucion, const vector<Node*>& depots, double capacidad) {
    double fo_total = 0.0;
    for (const Node* d : depots) {
        auto it = solucion.find(d->id);
        if (it != solucion.end()) {
            for (const auto& ruta : it->second) {
                if (ruta.empty()) continue;
                EvalResult eval = evaluar_ruta(ruta, d, capacidad);
                fo_total += calcular_fo_ruta(eval);
            }
        }
    }
    return fo_total;
}

void leer_instancia(const string& ruta_archivo, vector<Node*>& depots, vector<Node*>& clientes, int& num_vehiculos, double& capacidad) {
    ifstream file(ruta_archivo);
    if (!file.is_open()) {
        throw runtime_error("ERROR: No se encontro la ruta '" + ruta_archivo + "'. Revisa el directorio.");
    }

    string linea;
    int num_depots = 0;
    
    while (getline(file, linea)) {
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        if (linea.empty()) continue;
        
        if (linea.find("DEPOSITOS:") == 0 && linea.find("VEHICULOS_POR_DEPOSITO:") != string::npos) {
            stringstream ss(linea);
            string token;
            ss >> token >> token;
            num_depots = stoi(token);
            ss >> token >> token;
            num_vehiculos = stoi(token);
            ss >> token >> token;
            capacidad = stod(token);
        } else if (linea == "DEPOSITOS") {
            getline(file, linea);
            for (int i = 0; i < num_depots; ++i) {
                if (getline(file, linea)) {
                    if (!linea.empty() && linea.back() == '\r') linea.pop_back();
                    stringstream ss(linea);
                    string id; double x, y, demand, e_i, l_i, s_i;
                    if (ss >> id >> x >> y >> demand >> e_i >> l_i >> s_i)
                        depots.push_back(new Node(id, x, y, demand, e_i, l_i, s_i, true));
                }
            }
        } else if (linea == "CLIENTES") {
            getline(file, linea);
            while (getline(file, linea)) {
                if (!linea.empty() && linea.back() == '\r') linea.pop_back();
                if (linea.empty()) continue;
                stringstream ss(linea);
                string id; double x, y, demand, e_i, l_i, s_i;
                if (ss >> id >> x >> y >> demand >> e_i >> l_i >> s_i)
                    clientes.push_back(new Node(id, x, y, demand, e_i, l_i, s_i, false));
            }
            break;
        }
    }
    file.close();
}

// ============================================================
// FIX 1 + FIX 2: Greedy que valida ventanas de tiempo al insertar
// ============================================================
Solucion generar_solucion_inicial_greedy(const vector<Node*>& depots, const vector<Node*>& clientes, int num_vehiculos, double capacidad) {
    vector<Node*> clientes_ordenados = clientes;

    auto dist_min_depot = [&](const Node* c) {
        double min_d = 1e18;
        for (const Node* d : depots)
            min_d = min(min_d, calcular_distancia(c, d));
        return min_d;
    };

    // Score combinado: urgencia temporal + proximidad geografica
    sort(clientes_ordenados.begin(), clientes_ordenados.end(), [&](const Node* a, const Node* b) {
        double score_a = a->l_i - 0.5 * dist_min_depot(a);
        double score_b = b->l_i - 0.5 * dist_min_depot(b);
        return score_a < score_b;
    });

    Solucion solucion;
    map<string, vector<double>> cargas;

    for (const Node* d : depots) {
        solucion[d->id] = vector<vector<Node*>>(num_vehiculos);
        cargas[d->id] = vector<double>(num_vehiculos, 0.0);
    }

    for (Node* cliente : clientes_ordenados) {
        bool asignado = false;

        // FIX 1: Intentar insertar solo en rutas sin violacion de ventana de tiempo
        for (const Node* d : depots) {
            if (asignado) break;
            for (int v = 0; v < num_vehiculos; ++v) {
                if (cargas[d->id][v] + cliente->demand > capacidad) continue;

                vector<Node*> ruta_test = solucion[d->id][v];
                ruta_test.push_back(cliente);
                EvalResult eval_test = evaluar_ruta(ruta_test, d, capacidad);

                // Solo insertar si no genera tardanza nueva
                bool sin_tardanza = (eval_test.tardanza_total == 0.0);
                if (sin_tardanza) {
                    solucion[d->id][v].push_back(cliente);
                    cargas[d->id][v] += cliente->demand;
                    asignado = true;
                    break;
                }
            }
        }

        // FIX 1: Fallback — si no hay ruta factible, insertar en la de menor FO adicional
        if (!asignado) {
            double mejor_fo = 1e18;
            string mejor_d_id;
            int mejor_v = -1;

            for (const Node* d : depots) {
                for (int v = 0; v < num_vehiculos; ++v) {
                    if (cargas[d->id][v] + cliente->demand > capacidad) continue;

                    vector<Node*> ruta_test = solucion[d->id][v];
                    ruta_test.push_back(cliente);
                    EvalResult eval_test = evaluar_ruta(ruta_test, d, capacidad);
                    double fo = calcular_fo_ruta(eval_test);

                    if (fo < mejor_fo) {
                        mejor_fo = fo;
                        mejor_d_id = d->id;
                        mejor_v = v;
                    }
                }
            }

            if (mejor_v != -1) {
                solucion[mejor_d_id][mejor_v].push_back(cliente);
                cargas[mejor_d_id][mejor_v] += cliente->demand;
            }
        }
    }
    return solucion;
}

void escribir_solucion(const string& ruta_salida, int semilla, double tiempo_computo, const Solucion& solucion, const vector<Node*>& depots, double capacidad) {
    ofstream file(ruta_salida);
    if (!file.is_open()) return;

    double distancia_global = 0.0;
    double tiempo_global = 0.0;
    int vehiculos_usados = 0;
    vector<string> detalles_rutas;
    vector<string> detalles_penalizaciones;

    for (const Node* d : depots) {
        auto it = solucion.find(d->id);
        if (it == solucion.end()) continue;

        const auto& rutas = it->second;
        for (size_t v_idx = 0; v_idx < rutas.size(); ++v_idx) {
            const auto& ruta = rutas[v_idx];
            if (ruta.empty()) continue;

            vehiculos_usados++;
            EvalResult eval = evaluar_ruta(ruta, d, capacidad);
            distancia_global += eval.dist;
            tiempo_global += eval.tiempo;

            string secuencia_ids = "";
            for (size_t i = 0; i < ruta.size(); ++i) {
                secuencia_ids += ruta[i]->id;
                if (i < ruta.size() - 1) secuencia_ids += " -> ";
            }

            stringstream detalle;
            detalle << fixed << setprecision(2);
            detalle << "Deposito " << d->id << " (X=" << d->x << ", Y=" << d->y << ")\n"
                    << "Vehiculo " << (v_idx + 1) << ":\n"
                    << "Ruta: " << d->id << " -> " << secuencia_ids << " -> " << d->id << "\n"
                    << "Carga: " << eval.carga << " / " << capacidad << "\n"
                    << "Distancia: " << eval.dist << "\n"
                    << "Tiempo: " << eval.tiempo << " seg\n";
            detalles_rutas.push_back(detalle.str());

            for (const Penalty& p : eval.penalizaciones) {
                stringstream pen_str;
                pen_str << fixed << setprecision(2);
                if (p.is_capacity) {
                    pen_str << "Exceso de carga en ruta " << (v_idx + 1) << " (Dep " << d->id << "): " << p.exceso;
                } else {
                    pen_str << "Cliente " << p.id << ": llegada=" << p.llegada
                            << ", ventana=[" << p.e_i << ", " << p.l_i << "], tardanza=" << p.tardanza;
                }
                detalles_penalizaciones.push_back(pen_str.str());
            }
        }
    }

    // FIX 2: FO del archivo de salida tambien usa penalizacion proporcional
    double tardanza_total_global = 0.0;
    double exceso_total_global = 0.0;
    for (const Node* d : depots) {
        auto it = solucion.find(d->id);
        if (it == solucion.end()) continue;
        for (const auto& ruta : it->second) {
            if (ruta.empty()) continue;
            EvalResult eval = evaluar_ruta(ruta, d, capacidad);
            tardanza_total_global += eval.tardanza_total;
            exceso_total_global += eval.exceso_carga;
        }
    }
    double valor_fo = distancia_global + tardanza_total_global * 100.0 + exceso_total_global * 500.0;

    file << fixed << setprecision(2);
    file << "Semilla: " << semilla << "\n";
    file << "Tiempo de computo: " << setprecision(4) << tiempo_computo << " seg\n";
    file << "Valor Funcion objetivo: " << setprecision(2) << valor_fo << "\n\n";
    file << "Distancia total recorrida: " << distancia_global << "\n";
    file << "Tiempo total de viaje: " << tiempo_global << "\n";
    file << "Vehiculos usados: " << vehiculos_usados << "\n\n";
    file << "Solucion:\n";
    file << "----------------------------------------\n";

    for (const string& dr : detalles_rutas) file << dr << "\n";

    if (!detalles_penalizaciones.empty()) {
        file << "=== Nodos penalizados ===\n";
        for (const string& dp : detalles_penalizaciones) file << dp << "\n";
    }
    file.close();
}

enum TipoMovimiento { SWAP, RELOCATE, DOS_OPT };

void optimizar_hc(Solucion& solucion_actual, double& fo_actual, const vector<Node*>& depots, double capacidad, TipoMovimiento mov) {
    fo_actual = calcular_fo_total(solucion_actual, depots, capacidad);
    bool mejora = true;
    // FIX 3: Limite aumentado a 1000 para no cortar mejoras legitimas
    int iteraciones = 0;

    while (mejora && iteraciones < 1000) {
        mejora = false;

        double mejor_delta = 0.0;
        string mejor_d_id = "";
        int mejor_v_idx = -1;
        vector<Node*> mejor_ruta_encontrada;

        for (const Node* d : depots) {
            auto& rutas = solucion_actual[d->id];
            for (size_t v_idx = 0; v_idx < rutas.size(); ++v_idx) {
                auto& ruta = rutas[v_idx];
                if (ruta.size() < 2) continue;

                EvalResult eval_orig = evaluar_ruta(ruta, d, capacidad);
                double fo_ruta_orig = calcular_fo_ruta(eval_orig);  // FIX 2

                for (size_t i = 0; i < ruta.size(); ++i) {
                    size_t j_start = (mov == RELOCATE) ? 0 : i + 1;

                    for (size_t j = j_start; j < ruta.size(); ++j) {
                        if (i == j) continue;

                        vector<Node*> vec_ruta = ruta;

                        if (mov == SWAP) {
                            swap(vec_ruta[i], vec_ruta[j]);
                        } else if (mov == RELOCATE) {
                            // FIX 1 original: ajustar indice tras erase
                            Node* nodo = vec_ruta[i];
                            vec_ruta.erase(vec_ruta.begin() + i);
                            size_t insert_pos = (j > i) ? j - 1 : j;
                            vec_ruta.insert(vec_ruta.begin() + insert_pos, nodo);
                        } else if (mov == DOS_OPT) {
                            reverse(vec_ruta.begin() + i, vec_ruta.begin() + j + 1);
                        }

                        EvalResult eval_vecino = evaluar_ruta(vec_ruta, d, capacidad);
                        double fo_ruta_vecino = calcular_fo_ruta(eval_vecino);  // FIX 2

                        double delta = fo_ruta_vecino - fo_ruta_orig;

                        if (delta < mejor_delta) {
                            mejor_delta = delta;
                            mejor_ruta_encontrada = vec_ruta;
                            mejor_d_id = d->id;
                            mejor_v_idx = v_idx;
                        }
                    }
                }
            }
        }

        if (mejor_delta < -0.0001) {
            solucion_actual[mejor_d_id][mejor_v_idx] = mejor_ruta_encontrada;
            fo_actual += mejor_delta;
            mejora = true;
            iteraciones++;
        }
    }
}

bool relocate_inter_ruta(Solucion& solucion, double& fo_actual, const vector<Node*>& depots, double capacidad) {
    double mejor_delta = 0.0;
    string mejor_d1_id = "", mejor_d2_id = "";
    int mejor_v1 = -1, mejor_v2 = -1, mejor_i = -1, mejor_j = -1;

    for (const Node* d1 : depots) {
        auto& rutas1 = solucion[d1->id];
        for (int v1 = 0; v1 < (int)rutas1.size(); v1++) {
            auto& ruta1 = rutas1[v1];
            if (ruta1.size() < 2) continue;

            EvalResult eval1_orig = evaluar_ruta(ruta1, d1, capacidad);
            double fo1_orig = calcular_fo_ruta(eval1_orig);  // FIX 2

            for (int i = 0; i < (int)ruta1.size(); i++) {
                vector<Node*> ruta1_sin = ruta1;
                ruta1_sin.erase(ruta1_sin.begin() + i);
                EvalResult eval1_sin = evaluar_ruta(ruta1_sin, d1, capacidad);
                double fo1_sin = calcular_fo_ruta(eval1_sin);  // FIX 2
                double ganancia = fo1_orig - fo1_sin;

                for (const Node* d2 : depots) {
                    auto& rutas2 = solucion[d2->id];
                    for (int v2 = 0; v2 < (int)rutas2.size(); v2++) {
                        if (d1->id == d2->id && v1 == v2) continue;

                        auto& ruta2 = rutas2[v2];
                        EvalResult eval2_orig = evaluar_ruta(ruta2, d2, capacidad);
                        double fo2_orig = calcular_fo_ruta(eval2_orig);  // FIX 2

                        for (int j = 0; j <= (int)ruta2.size(); j++) {
                            vector<Node*> ruta2_con = ruta2;
                            ruta2_con.insert(ruta2_con.begin() + j, ruta1[i]);
                            EvalResult eval2_con = evaluar_ruta(ruta2_con, d2, capacidad);
                            double fo2_con = calcular_fo_ruta(eval2_con);  // FIX 2
                            double costo = fo2_con - fo2_orig;

                            double delta = costo - ganancia;
                            if (delta < mejor_delta) {
                                mejor_delta = delta;
                                mejor_d1_id = d1->id; mejor_d2_id = d2->id;
                                mejor_v1 = v1; mejor_v2 = v2;
                                mejor_i = i; mejor_j = j;
                            }
                        }
                    }
                }
            }
        }
    }

    if (mejor_v1 != -1) {
        Node* nodo = solucion[mejor_d1_id][mejor_v1][mejor_i];
        solucion[mejor_d1_id][mejor_v1].erase(solucion[mejor_d1_id][mejor_v1].begin() + mejor_i);
        solucion[mejor_d2_id][mejor_v2].insert(solucion[mejor_d2_id][mejor_v2].begin() + mejor_j, nodo);
        fo_actual += mejor_delta;
        return true;
    }
    return false;
}

int main() {
    std::filesystem::create_directories("outputs");
    vector<string> instancias = {"C101.txt", "R201.txt", "RC101.txt"};
    string directorio_base = "instancias/";

    for (const string& nombre_instancia : instancias) {
        string ruta_archivo = directorio_base + nombre_instancia;
        string prefijo = nombre_instancia.substr(0, nombre_instancia.find("."));

        vector<Node*> depots;
        vector<Node*> clientes;
        int num_vehiculos = 0;
        double capacidad = 0.0;

        cout << "=========================================\n";
        cout << "PROCESANDO INSTANCIA: " << nombre_instancia << "\n";
        cout << "=========================================\n";

        try {
            leer_instancia(ruta_archivo, depots, clientes, num_vehiculos, capacidad);

            Solucion sol_inicial = generar_solucion_inicial_greedy(depots, clientes, num_vehiculos, capacidad);
            double fo_inicial = calcular_fo_total(sol_inicial, depots, capacidad);
            cout << "FO Inicial (Greedy): " << fixed << setprecision(2) << fo_inicial << "\n\n";

            Solucion sol_swap     = sol_inicial;
            Solucion sol_relocate = sol_inicial;
            Solucion sol_2opt     = sol_inicial;

            double fo_swap = 0.0, fo_relocate = 0.0, fo_2opt = 0.0;

            // --- Ejecucion SWAP ---
            auto start_s = chrono::high_resolution_clock::now();
            optimizar_hc(sol_swap, fo_swap, depots, capacidad, SWAP);
            while (relocate_inter_ruta(sol_swap, fo_swap, depots, capacidad));
            chrono::duration<double> diff_s = chrono::high_resolution_clock::now() - start_s;
            escribir_solucion("outputs/salida_swap_" + prefijo + ".txt", 0, diff_s.count(), sol_swap, depots, capacidad);
            cout << "[SWAP]     FO Final: " << fo_swap << " | Tiempo: " << diff_s.count() << "s\n";

            // --- Ejecucion RELOCATE ---
            auto start_r = chrono::high_resolution_clock::now();
            optimizar_hc(sol_relocate, fo_relocate, depots, capacidad, RELOCATE);
            while (relocate_inter_ruta(sol_relocate, fo_relocate, depots, capacidad));
            chrono::duration<double> diff_r = chrono::high_resolution_clock::now() - start_r;
            escribir_solucion("outputs/salida_relocate_" + prefijo + ".txt", 0, diff_r.count(), sol_relocate, depots, capacidad);
            cout << "[RELOCATE] FO Final: " << fo_relocate << " | Tiempo: " << diff_r.count() << "s\n";

            // --- Ejecucion 2-OPT ---
            auto start_2 = chrono::high_resolution_clock::now();
            optimizar_hc(sol_2opt, fo_2opt, depots, capacidad, DOS_OPT);
            while (relocate_inter_ruta(sol_2opt, fo_2opt, depots, capacidad));
            chrono::duration<double> diff_2 = chrono::high_resolution_clock::now() - start_2;
            escribir_solucion("outputs/salida_2opt_" + prefijo + ".txt", 0, diff_2.count(), sol_2opt, depots, capacidad);
            cout << "[2-OPT]    FO Final: " << fo_2opt << " | Tiempo: " << diff_2.count() << "s\n\n";

        } catch (const exception& e) {
            cerr << e.what() << "\n\n";
        }

        for (Node* d : depots) delete d;
        for (Node* c : clientes) delete c;
    }

    return 0;
}