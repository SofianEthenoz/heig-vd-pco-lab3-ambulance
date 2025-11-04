#include "insurance.h"
#include "costs.h"
#include <pcosynchro/pcomutex.h>
#include <pcosynchro/pcothread.h>


Insurance::Insurance(int uniqueId, int fund) : Seller(fund, uniqueId) {}

void Insurance::run() {
    logger() << "Insurance " <<  uniqueId << " starting with fund " << money << std::endl;

    while (true) {
        clock->worker_wait_day_start();

        // TODO condition d'arrêt
        if (PcoThread::thisThread()->stopRequested()) break;

        // Réception de la somme des cotisations journalières des assurés
        receiveContributions();

        // Payer les factures
        payBills();

        clock->worker_end_day();
    }

    logger() << "Insurance " <<  uniqueId << " stopping with fund " << money << std::endl;
}

void Insurance::receiveContributions() {
    // TODO
    mutex.lock();

    this->money += INSURANCE_CONTRIBUTION;

    mutex.unlock();
}

void Insurance::invoice(int bill, Seller* who) {
    // TODO
    // protéger l'accès concurrent à la liste des factures impayées
    mutex.lock();

    // on ajoute la facture à la liste des factures impayées
    this->unpaidBills.emplace_back(who, bill);

    // libérer le mutex
    mutex.unlock();
}

void Insurance::payBills() {
    // TODO
    std::vector<std::pair<Seller*, int>> toPay;

    // protège l'accès conccurent avec unpaidBills et les autres variables partagées
    mutex.lock();
    toPay = unpaidBills;

    // on retire les factures payées de la liste des factures impayées
    unpaidBills.clear();
    mutex.unlock();

    for (auto& bill : toPay) {
        if (!bill.first) continue;

        mutex.lock();
        if (money < bill.second) {
            unpaidBills.push_back(bill);
            mutex.unlock();
            continue;
        }

        // on retire le montant de la facture des fonds de l'assurance
        money -= bill.second;
        mutex.unlock();

        // on paie la facture au Seller
        bill.first->pay(bill.second);
    }
}

