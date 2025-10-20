#include "insurance.h"
#include "costs.h"
#include <pcosynchro/pcomutex.h>
#include <pcosynchro/pcothread.h>


Insurance::Insurance(int uniqueId, int fund) : Seller(fund, uniqueId) {}

static PcoMutex mutex;

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
    this->money += INSURANCE_CONTRIBUTION;
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
    int paid = 0;
    
    for (std::pair<Seller*, int> bill : this->unpaidBills)
    {
        if(money >= bill.second)
        {
            // on paie la facture au Seller
            bill.first->pay(bill.second);

            // on retire le montant de la facture des fonds de l'assurance
            this->money -= bill.second;

            paid++;
        } else {
            break;
        }
    }

    // on retire les factures payées de la liste des factures impayées
    this->unpaidBills.erase(this->unpaidBills.begin(), this->unpaidBills.begin() + paid);
}
