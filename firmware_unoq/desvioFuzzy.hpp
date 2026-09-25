#ifndef DESVIOFYZZY_HPP
#define DESVIOFYZZY_HPP

#include "ultrassom.hpp"
#include <Fuzzy.h>


//incializando objeto fuzzy
Fuzzy *fuzzy = new Fuzzy();

void addRegra(int id, FuzzySet *frente, FuzzySet *direita, FuzzySet *esquerda, 
            FuzzySet *direcao, FuzzySet *velocidade);

// Variantes: regra que so comanda velocidade, e regra que so comanda direcao.
void addRegraVel(int id, FuzzySet *frente, FuzzySet *direita, FuzzySet *esquerda,
            FuzzySet *velocidade);
void addRegraCone(int id, FuzzySet *frente, FuzzySet *cone, FuzzySet *direcao);

void inicializarFuzzy()
{
    //inicializando os objetos de entrada fuzzy
    FuzzyInput *distanciaDireita    = new FuzzyInput(1);
    FuzzyInput *distanciaEsquerda   = new FuzzyInput(2);
    FuzzyInput *distanciaFrente     = new FuzzyInput(3);
    FuzzyInput *posicaoCone         = new FuzzyInput(4);
    
    //inicializando um fuzzyset object
    FuzzySet *pertoFr = new FuzzySet(0, 0, 20, 45);
    FuzzySet *medioFr = new FuzzySet(30, 50, 70, 110);
    FuzzySet *longeFr = new FuzzySet(80, 150, 400, 400);
    
    FuzzySet *pertoDi   = new FuzzySet(0, 0, 30, 80);
    //FuzzySet *medioDi = new FuzzySet(30, 50, 70, 110); comentado para reduzir a complexidade
    FuzzySet *longeDi   = new FuzzySet(30, 80, 400, 400);
    
    FuzzySet *pertoEs   = new FuzzySet(0, 0, 30, 80);
    //FuzzySet *medioEs = new FuzzySet(30, 50, 70, 110); 
    FuzzySet *longeEs   = new FuzzySet(30, 80, 400, 400);
    
    /* Posicao do cone, como a camera manda: -255 = cone todo a esquerda,
     * +255 = cone todo a direita, 0 = centralizado.
     *
     * ATENCAO ao sinal: esta convencao e OPOSTA a da saida de direcao, onde
     * positivo aciona PIN_ESQ. A inversao esta feita nas REGRAS (coneDir aponta
     * para dirSuave), nao em conta aritmetica. Nao inverta de novo no .ino. */
    FuzzySet *coneEsq    = new FuzzySet(-255, -255, -120, -20);
    FuzzySet *coneCentro = new FuzzySet( -60,    0,    0,   60);
    FuzzySet *coneDir    = new FuzzySet(  20,  120,  255,  255);

    posicaoCone->addFuzzySet(coneEsq);
    posicaoCone->addFuzzySet(coneCentro);
    posicaoCone->addFuzzySet(coneDir);

    //adicionando os fuzzyset as entradas
    distanciaDireita->addFuzzySet(pertoDi);
    //distanciaDireita->addFuzzySet(medioDi);
    distanciaDireita->addFuzzySet(longeDi);
    
    distanciaEsquerda->addFuzzySet(pertoEs);
    //distanciaEsquerda->addFuzzySet(medioEs);
    distanciaEsquerda->addFuzzySet(longeEs);
    
    distanciaFrente->addFuzzySet(pertoFr);
    distanciaFrente->addFuzzySet(medioFr);
    distanciaFrente->addFuzzySet(longeFr);
    
    //adicionando objetos ao fuzzy
    fuzzy->addFuzzyInput(distanciaDireita);
    fuzzy->addFuzzyInput(distanciaEsquerda);
    fuzzy->addFuzzyInput(distanciaFrente);
    fuzzy->addFuzzyInput(posicaoCone);

    //inicializando objetos de saida fuzzy
    FuzzyOutput *velocidade = new FuzzyOutput(1);
    FuzzyOutput *direcao    = new FuzzyOutput(2);

    FuzzySet *lenta   = new FuzzySet(  0,   0, 120, 170);
    FuzzySet *media   = new FuzzySet(140, 185, 185, 220);
    FuzzySet *rapida  = new FuzzySet(190, 230, 255, 255);

    velocidade->addFuzzySet(lenta);
    velocidade->addFuzzySet(media);
    velocidade->addFuzzySet(rapida);

    FuzzySet *dirForte = new FuzzySet(-255, -255, -210, -130);
    FuzzySet *dirSuave = new FuzzySet(-200, -150, -150,  -90);
    FuzzySet *centro   = new FuzzySet( -70,    0,    0,   70);
    FuzzySet *esqSuave = new FuzzySet(  90,  150,  150,  200);
    FuzzySet *esqForte = new FuzzySet( 130,  210,  255,  255);

    direcao->addFuzzySet(dirForte);
    direcao->addFuzzySet(dirSuave);
    direcao->addFuzzySet(centro);
    direcao->addFuzzySet(esqSuave);
    direcao->addFuzzySet(esqForte);

    fuzzy->addFuzzyOutput(velocidade);
    fuzzy->addFuzzyOutput(direcao);

    //*************************Regras******************************

    /* A regra 1 (tudo livre) nao comanda mais direcao: quem esterça nesse caso
     * e o cone, pelas regras 13-15. Se ela mantivesse 'centro', puxaria o
     * esterço para zero e brigaria com o seguimento do cone o tempo todo. */
    addRegraVel(1, longeFr, longeDi, longeEs, rapida);
    addRegra(2, longeFr, longeDi, pertoEs, dirSuave, rapida);
    addRegra(3, longeFr, pertoDi, longeEs, esqSuave, rapida);
    addRegra(4, longeFr, pertoDi, pertoEs, centro, media);

    addRegra(5, medioFr, longeDi, longeEs, centro, media);
    addRegra(6, medioFr, longeDi, pertoEs, dirSuave, media);
    addRegra(7, medioFr, pertoDi, longeEs, esqSuave, media);
    addRegra(8, medioFr, pertoDi, pertoEs, centro, lenta);

    addRegra(9, pertoFr, longeDi, longeEs, esqForte, lenta);
    addRegra(10, pertoFr, longeDi, pertoEs, dirForte, lenta);
    addRegra(11, pertoFr, pertoDi, longeEs, esqForte, lenta);
    addRegra(12, pertoFr, pertoDi, pertoEs, centro, lenta);

    /* Seguimento do cone, so com a frente livre. Com obstaculo proximo, quem
     * manda na direcao sao as regras 5-12, que usam os conjuntos FORTE e
     * dominam a composicao. O cone usa so SUAVE, de proposito: emergencia
     * ganha de perseguicao. */
    addRegraCone(13, longeFr, coneEsq,    esqSuave);
    addRegraCone(14, longeFr, coneCentro, centro);
    addRegraCone(15, longeFr, coneDir,    dirSuave);
}

//template para as regras de fuzzy
void addRegra(int id, FuzzySet *frente, FuzzySet *direita, FuzzySet *esquerda, 
            FuzzySet *direcao, FuzzySet *velocidade)
{
    FuzzyRuleAntecedent *a1 = new FuzzyRuleAntecedent();
    a1->joinWithAND(frente, direita);

    FuzzyRuleAntecedent *a2 = new FuzzyRuleAntecedent();
    a2->joinWithAND(a1, esquerda);

    FuzzyRuleConsequent *c = new FuzzyRuleConsequent();
    c->addOutput(direcao);
    c->addOutput(velocidade);

    fuzzy->addFuzzyRule(new FuzzyRule(id, a2, c));
}

void addRegraVel(int id, FuzzySet *frente, FuzzySet *direita, FuzzySet *esquerda,
            FuzzySet *velocidade)
{
    FuzzyRuleAntecedent *a1 = new FuzzyRuleAntecedent();
    a1->joinWithAND(frente, direita);

    FuzzyRuleAntecedent *a2 = new FuzzyRuleAntecedent();
    a2->joinWithAND(a1, esquerda);

    FuzzyRuleConsequent *c = new FuzzyRuleConsequent();
    c->addOutput(velocidade);

    fuzzy->addFuzzyRule(new FuzzyRule(id, a2, c));
}

void addRegraCone(int id, FuzzySet *frente, FuzzySet *cone, FuzzySet *direcao)
{
    FuzzyRuleAntecedent *a = new FuzzyRuleAntecedent();
    a->joinWithAND(frente, cone);

    FuzzyRuleConsequent *c = new FuzzyRuleConsequent();
    c->addOutput(direcao);

    fuzzy->addFuzzyRule(new FuzzyRule(id, a, c));
}

#endif 
