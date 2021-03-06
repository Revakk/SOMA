
#include "stdafx.h"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <random>
#include "Functions.h"//import knihoven


//konstanty SF = mutace, CR = krizeni
#define SF 0.5 
#define CR 0.1
constexpr float step = 0.11;
constexpr float pathLength = 3.0;
constexpr float prt = 0.1;

//vyber funcke
#define function zakharov

//definice barev
#define black cv::Scalar(0, 0, 0) 
#define gray cv::Scalar(150,150,150)
#define red cv::Scalar(0,0,200)
#define blue cv::Scalar(200,100,50)

//velikost okna
int ws_x = 1000;
int ws_y = 1000;


//vykresleni kruhu v danem bode s danou barvou
void MyFilledCircle(cv::Mat& img, cv::Point center, cv::Scalar color)
{
    circle(img,
        center,
        3,
        color,
        cv::FILLED,
        cv::LINE_8);
}

// funkce ktera navraci nejvyssi moznou hodnotu funkce, pro vykresleni pozadi 
template<typename F>//predavam funkci jako argument pomoci template
void testRanges(cv::Mat& img, F func, float& maxVal)
{
    std::vector<float> vec(2);
    vec.emplace_back(0);
    vec.emplace_back(0);
    float curVal;
    for (float y = 0; y < ws_y; y++)
    {
        for (float x = 0; x < ws_x; x++)//iteruji pres jednotlive pixely a zjistuji v jake hodnote ma funkce nejvetsi hodnotu
        {
            vec[0] = x / ws_x;
            vec[1] = y / ws_y;
            curVal = func(vec);
            if (curVal > maxVal)
            {
                maxVal = curVal;
            }
        }
    }
}


template<typename F>
static void colorBackground(cv::Mat& img, float& maxVal, F func)//funkce ktera obarvi pozadi pro urychleni nebarvim po pixelu ale po obdelniku 5x5
{
    std::vector<float> vec(2);
    vec.emplace_back(0);
    vec.emplace_back(0);
    float value;
    for (float y = 0; y < ws_y; y += 5)
    {
        for (float x = 0; x < ws_x; x += 5)
        {
            vec[0] = x / ws_x;
            vec[1] = y / ws_y;
            value = func(vec) / maxVal;
            cv::rectangle(img, cv::Point(x, y), cv::Point(x + 5, y + 5), cv::Scalar(255.0 * value, 255.0 * value, 255.0 * value), -1);
        }
    }
}


struct individual//struktura ktera popisuje jedince jeho pozici, fitness, krok, a udaje o nejlepsim kroku
{
    std::vector<float> position;
    float fitness = 0;
    std::vector<float> step;
    std::vector<float> bestStep;
    float bestStepFitness;

    individual() : position(std::vector<float>(dimension)), fitness(0), step(std::vector<float>(dimension)), bestStep(std::vector<float>(dimension)),bestStepFitness(0) {}
};


inline int rngGen(const int& a, const int& b)//funkce pro generaci nahodnych cisel v rozmezi a-b pro cele cisla
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<int> dist6(a, b);

    return dist6(rng);
}


inline float rngGen(const float& a, const float& b)//funkce pro generaci nahodnych cisel v rozmezi a-b pro float cisla
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<float> dist6(a, b);

    return (dist6(rng));
}

void generatePopulation(std::vector<individual>& population, const int& NP)//funcke pro generovani populace (nahodna pozice)
{
    population.reserve(NP);//reservuji misto a tim omezim pocet kopirovani
    for (int i = 0; i < NP; i++)//naplnim populaci prazdnymi jedinci
    {
        population.emplace_back(individual());
        std::cout << "emplace_back individual" << std::endl;
    }

    for (int i = 0; i < NP; i++)
    {
        for (int j = 0; j < dimension; j++)
        {
            population[i].position[j] = (rngGen(0.0f, 1.0f));
        }
        
    }
}

//funkce pro vykresleni populace
void drawPopulation(std::vector<individual>& population, const int& NP, cv::Mat& image, individual & leader)
{
    for (int i = 0; i < NP; i++)
    {
        MyFilledCircle(image, cv::Point(population[i].position[0] * float(ws_x), population[i].position[1] * float(ws_x)), blue);
    }
    MyFilledCircle(image, cv::Point(leader.position[0] * float(ws_x), leader.position[1] * float(ws_x)), red);
    // cv::waitKey(1);
}

//funkce projde vsechny jedince a vypocita se jejich puvodni fitness
template<typename F>
void calculateFitness(std::vector<individual>& pop, const int NP, F func, individual & leader)
{
    for (int i = 0; i < NP; i++)
    {
        pop[i].fitness = func(pop[i].position);
        pop[i].bestStep = pop[i].position;
        pop[i].bestStepFitness = pop[i].fitness;
        //pop[i].fitness = ackley(pop[i].position);
        if (leader.fitness >= pop[i].fitness)
        {
            leader.fitness = pop[i].fitness;
            leader.position = pop[i].position;
        }
    }
}


int getBestIndividualIndex(std::vector<individual>& pop, const int NP, float& bestFitness, int& bestIndex)//funkce navrati nejlepsiho jedince dane populace(respektive jeho index ve vektoru)
{
    for (int i = 0; i < NP; i++)
    {
        if (bestFitness >= pop[i].fitness)
        {
            bestFitness = pop[i].fitness;
            bestIndex = i;
        }

    }
    return bestIndex;
}


template<typename T>
void makeStep(std::vector<individual>& tr, const individual& leader, std::vector<float> & prtV, T func)//funcke provede krok u vsech jedincu
{
    bool isZero = true;
    int val;
    float t = step;
    float value;
    while (t <= pathLength)//dokud je krok mensi nez celkova cesta
    {
        for (auto&& ind : tr)//projdu vsechny jedince
        {
            for (int i = 0; i < dimension; i++)//projdu velikost dimenze a nahodne vygerneruji cislo
            {
                if (rngGen(0.0f, 1.0f) < prt)//pokud je cislo mensi nez promenna prt tak ulozim 1
                {
                    prtV[i] = 1;
                    isZero = false;
                }
                else//jinak ukladam 0
                {
                    prtV[i] = 0;
                }
            }
            if (isZero)//pokud je prtVector prazdny tak nahodne vygeneruju cislo podle ktereho prtV naplnim jednickami
            {
                val = rngGen(0, dimension);
                if (val == dimension)
                {
                    for (auto& p : prtV)
                    {
                        p = 1;
                    }
                }
                else
                {
                    prtV[val] = 1;
                }

            }
            isZero = true;
            //std::cout << val << std::endl;
            //std::cout << "prtv[0]: " << prtV[0] << " prtv[1]:" << prtV[1] << std::endl;
            for (int i = 0; i < dimension; i++)//projdu parametry jedincu a provedu krok
            {
                ind.step[i] = ind.position[i] + (leader.position[i] - ind.position[i]) * t * prtV[i];
                if (ind.step[i] >= 1)//zkontroluji zda jedinec neprekrocil meze
                {
                    ind.step[i] = 1;
                }
                if (ind.step[i] < 0)
                {
                    ind.step[i] = 0;
                }
            }
            value = func(ind.step);
            //std::cout << "value:" << value << std::endl;
            if (value <= ind.bestStepFitness)//krok ktery je lepsi nebo stejne dobry nahradim 
            {
                ind.bestStep = ind.step;
                ind.bestStepFitness = value;
            }
        }
        t += step;
       
    }
}
void updatePos(std::vector<individual>& tr)//aktualizuji pozici jedincu na zaklade jejich kroku
{
    for (auto& ind : tr)
    {
        ind.position = ind.bestStep;
    }
}



int main()
{
    //cv::Mat image(ws_x, ws_y + 300, CV_8UC3, cv::Scalar(0, 0, 0));//vytvoreni obrazu pro kresleni
    //cv::Mat backGround;//matice ktera bude uchovavat pozadi
    const int NP = 30;//pocet jedincu populace
    individual mutant;//jedinec na ktereho bude uplatnena mutace
   // testRanges(image, function, maxVal);

    //colorBackground(image, maxVal, function);
   // backGround = image.clone();//ulozeni pozadi do opencv matice
    //generatePopulation(currentPop, NP);//generace populace
    std::vector<individual> currentPop;
    individual leader;
    
    std::vector<float> prtV(dimension);
    int bestIndex;
    int migrations = 500;
    int currentMigration = 0;
    generatePopulation(currentPop, NP);
    calculateFitness(currentPop, NP, function, leader);
    leader.position = currentPop[0].position;
    leader.fitness = currentPop[0].fitness;

    while (currentMigration < migrations)
    {
         calculateFitness(currentPop, NP, function, leader);
         makeStep(currentPop, leader, prtV, function);
         updatePos(currentPop);
         currentMigration++;
         std::cout << leader.fitness << std::endl;
         //std::cout << "functioncalls:" << functionCalls << std::endl;
    }
    return 0;
}