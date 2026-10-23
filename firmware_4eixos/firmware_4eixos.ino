/**
 * Firmware do MCU do Arduino Uno Q (STM32U585 / Zephyr).
 *
 * Recebe da CPU Linux, pela Bridge, a posicao do cone (gradiente) e o modo de
 * operacao, e aciona os dois motores: tracao e direcao.
 *
 * Este arquivo nao implementa comportamento. Ele faz tres coisas:
 *   1. recebe o comando da Bridge e guarda o estado
 *   2. traduz esse estado em um modo
 *   3. despacha o modo para o servico correspondente, aplica as travas e aciona
 *
 * Cada comportamento vive no seu proprio header e serve uma funcao com a mesma
 * forma -- entradas por parametro, vel e dir por referencia:
 *
 *   motores.hpp     acionarPonte, aplicarEsterco, pararMotores
 *   ultrassom.hpp   atualizarSonares, dist_1..3, houveMedicaoNova
 *   desvioFuzzy.hpp passoFuzzy       (modo 'a', aproximar)
 *   busca.hpp       passoBusca       (modo 'p', procurar)
 *   contorno.hpp    passoContorno    (modo 'c', contornar)  -- esqueleto
 *   travas.hpp      aplicarTravas                           -- esqueleto
 *   desenharLed.hpp atualizarLedModo
 *   mpu.hpp         lerMPU   (gated por USAR_MPU)
 *
 * Nenhum servico sabe em que modo foi chamado -- ele so sabe executar o proprio
 * comportamento. O despacho por modo existe so neste arquivo.
 *
 * USAR_MPU e a unica flag de compilacao que resta, e precisa ser definida ANTES
 * dos includes: o mpu.hpp depende dela para decidir se gera codigo.
 */


#include <Arduino_RouterBridge.h>

#include "motores.hpp"
#include "ultrassom.hpp"
#include "busca.hpp" //modo busca
#include "contorno.hpp" //modo contorno
#include "travas.hpp"
#include "mpu.hpp"
#include "desenharLed.hpp"