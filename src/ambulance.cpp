// ambulance.cpp
#include "ambulance.h"
#include "costs.h"
#include <pcosynchro/pcothread.h>


Ambulance::Ambulance(int id, int fund,
                     std::vector<ItemType> resourcesSupplied,
                     std::map<ItemType,int> initialStocks)
: Seller(fund, id), resourcesSupplied(resourcesSupplied) {
    for (auto it : resourcesSupplied) {
        stocks[it] = initialStocks.count(it) ? initialStocks[it] : 0;
    }
}

static PcoMutex mutex;

void Ambulance::run() {
    logger() << "Ambulance " <<  uniqueId << " starting with fund " << money << std::endl;

    while (true) {
        clock->worker_wait_day_start();
        if (PcoThread::thisThread()->stopRequested()) break;

        sendPatients();

        clock->worker_end_day();
    }

    logger() << "Ambulance " <<  uniqueId << " stopping with fund " << money << std::endl;
}

void Ambulance::sendPatients() {
    // Choisir un hôpital au hasard
    auto* hospital = chooseRandomSeller(hospitals);
    // Déterminer le nombre de patients à envoyer
    int nbPatientsToTransfer = 1 + rand() % 5;

    const int salary = getEmployeeSalary(EmployeeType::EmergencyStaff);

    // TODO
    // Conditions : doit avoir assez de patients et d'argent pour payer le salaire
    if (stocks[ItemType::SickPatient] < nbPatientsToTransfer || money < salary)
        return;

    // On paie le salaire du jour
    money -= salary;
    
    // On transfère les patients
    stocks[ItemType::SickPatient] -= nbPatientsToTransfer;
    hospital->transfer(ItemType::SickPatient, nbPatientsToTransfer);

    // On reçoit le paiement pour le transport
    money += nbPatientsToTransfer * getCostPerService(ServiceType::Transport);
}

void Ambulance::pay(int bill) {
    // TODO
    // protéger l'accès concurrent à la liste des factures impayées
    mutex.lock();

    // On raque
    money += bill;

    // libérer le mutex
    mutex.unlock();
}

void Ambulance::setHospitals(std::vector<Seller*> h) {
    hospitals = std::move(h);
}

void Ambulance::setInsurance(Seller* ins) { 
    insurance = ins; 
}

int Ambulance::getNumberPatients() {
    return stocks[ItemType::SickPatient];
}
