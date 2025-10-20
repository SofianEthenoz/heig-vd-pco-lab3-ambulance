#include "supplier.h"
#include "costs.h"
#include "seller.h"
#include <pcosynchro/pcothread.h>
#include <iostream>


Supplier::Supplier(int uniqueId, int fund, std::vector<ItemType> resourcesSupplied)
    : Seller(fund, uniqueId), resourcesSupplied(resourcesSupplied) {
    for (const auto& item : resourcesSupplied) {    
        stocks[item] = 0;    
    }
}

static PcoMutex mutex;

void Supplier::run() {
    logger() << "Supplier " <<  uniqueId << " starting with fund " << money << std::endl;

    while (true) {
        clock->worker_wait_day_start();
        
        // TODO condition d'arrêt
        if (PcoThread::thisThread()->stopRequested()) break;

        attemptToProduceResource();
        paySuppliersStaff();

        clock->worker_end_day();
    }

    logger() << "Supplier " <<  uniqueId << " stopping with fund " << money << std::endl;
}

void Supplier::paySuppliersStaff() {
    // TODO
    int totalSalary = getAmountPaidToEmployees(EmployeeType::Supplier);
    
    if (money >= totalSalary)
        money -= totalSalary;

    this->nbEmployeesPaid++;
}

void Supplier::attemptToProduceResource() {
    // TODO

    // Vérifier qu'il y a des ressources à produire
    if (resourcesSupplied.empty()) return;

    // Choisir un item à produire
    ItemType itemToProduce = Seller::chooseRandomItem(stocks);
    int costPerUnit = getCostPerUnit(itemToProduce);
    
    if (money >= costPerUnit) {
        // Payer le supplier
        money -= costPerUnit;

        // Ajouter l'article au stock
        stocks[itemToProduce] += 1;
    }
}

int Supplier::buy(ItemType it, int qty) {
    // TODO
    // Vérifier la validité de la demande

    // si la quantité est nulle ou négative 
    if (qty <= 0) return 0;
    // si le Supplier ne vend pas cette ressource
    if (!sellsResource(it)) return 0;
    // si le stock est insuffisant
    if (this->stocks[it] < qty) return 0;
    
    // Retire la quantité achetée du stock
    this->stocks[it] -= qty;

    // Retourne le coût unitaire multiplié par la quantité
    return getCostPerUnit(it) * qty;
}

void Supplier::pay(int bill) {
    // TODO
    // protéger l'accès concurrent à la liste des factures impayées
    mutex.lock();

    // Comme d'habitude on raque
    this->money += bill;

    // libérer le mutex
    mutex.unlock();
}

int Supplier::getMaterialCost() {
    int totalCost = 0;
    for (const auto& item : resourcesSupplied) {
        totalCost += getCostPerUnit(item);
    }
    return totalCost;
}

bool Supplier::sellsResource(ItemType item) const {
    return std::find(resourcesSupplied.begin(), resourcesSupplied.end(), item) != resourcesSupplied.end();
}
