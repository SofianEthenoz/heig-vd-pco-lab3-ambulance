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
    // TODO
    // Déterminer le nombre de patients à envoyer
    int nbPatientsToTransfer = 1 + rand() % 5;

    // Obtenir le salaire d'un EmergencyStaff
    const int salary = getEmployeeSalary(EmployeeType::EmergencyStaff);

    // protéger l'accès concurrent à money
    mutex.lock();

    // Pas de patients ou pas d'argent pour payer le salaire; on sort d'ici
    if (stocks[ItemType::SickPatient] <= 0 || money < salary) {
        mutex.unlock();
        return;
    }

    // Conditions doit avoir assez de patients à transférer
    if (nbPatientsToTransfer > stocks[ItemType::SickPatient]) nbPatientsToTransfer = stocks[ItemType::SickPatient];

    // Choisir un hôpital au hasard
    auto* hospital = chooseRandomSeller(hospitals);
     // On transfère les patients
    const int accepted = hospital->transfer(ItemType::SickPatient, nbPatientsToTransfer);

    // Si l'hôpital n'a rien accepté, on sort d'ici
    if(accepted <= 0) {
        mutex.unlock();
        return;
    }

    // On paie le salaire du jour
    money -= salary;
    nbEmployeesPaid += 1;

    mutex.unlock();
    
    // On met à jour le stock de patients dans l'ambulance
    stocks[ItemType::SickPatient] -= accepted;
    
    // On créait la facture
    this->insurance->invoice(accepted * getCostPerService(ServiceType::Transport), this);
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
