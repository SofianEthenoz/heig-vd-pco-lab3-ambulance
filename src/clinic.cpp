#include "clinic.h"
#include "costs.h"
#include <pcosynchro/pcothread.h>
#include <iostream>
#include <random>

Clinic::Clinic(int id, int fund, std::vector<ItemType> resourcesNeeded)
: Seller(fund, id), resourcesNeeded(std::move(resourcesNeeded)) {
    for (auto it : this->resourcesNeeded) {
        stocks[it] = 0;
    }

    stocks[ItemType::SickPatient] = 0;
    stocks[ItemType::RehabPatient] = 0;
}

void Clinic::run() {
    logger() << "Clinic " <<  uniqueId << " starting with fund " << money << std::endl;

    while (true) {
        clock->worker_wait_day_start();

        // TODO condition d'arrêt
        if (PcoThread::thisThread()->stopRequested()) break;

        // Essayer de traiter le prochain patient
        processNextPatient();

        // Transférer les patients déjà traités vers un hôpital pour leur réhabilitation
        sendPatientsToRehab();

        // Payer les factures en retard
        payBills();

        clock->worker_end_day();
    }

    logger() << "Clinic " <<  uniqueId << " stopping with fund " << money << std::endl;
}


int Clinic::transfer(ItemType what, int qty) {
    // TODO
    mutex.lock();

    // 1. La clinique n'accepte que des patients malades
    if (what != ItemType::SickPatient)
    {
        mutex.unlock();
        return 0;
    }

    // 2. Refuse si la clinique n’a plus de fonds    
    if (money <= 0)
    {
        mutex.unlock();
        return 0;
    }

    // 3. Refuser si elle a des factures impayées
    if (this->unpaidBills.size() > 0)
    {
        mutex.unlock();
        return 0;
    }

    // 4. Sinon accepter et ajouter les patients au stock
    stocks[what] += qty;

    mutex.unlock();
    return qty;
}

bool Clinic::hasResourcesForTreatment() const {
    // TODO
    int price = 0;

    // Calculer le coût total des ressources nécessaires des médicaments pour le traitement
    for (ItemType itemNeeded : this->resourcesNeeded)
        price += getCostPerUnit(itemNeeded);

    return (money >= price);
}

void Clinic::payBills() {
     // TODO
    std::vector<std::pair<Supplier*, int>> toPay;

    mutex.lock();
    toPay = this->unpaidBills;

    // on retire les factures de la liste des factures impayées
    this->unpaidBills.clear();
    mutex.unlock();

    for (auto& bill : toPay) {
        // on paie la facture au Supplier
        if (bill.first)
            bill.first->pay(bill.second);

        // on retire le montant de la facture des fonds de la clinique
        mutex.lock();
        this->money -= bill.second;
        mutex.unlock();
    }
}

void Clinic::processNextPatient() {
    // TODO
    treatOne();
}

void Clinic::sendPatientsToRehab() {
    // TODO
    int nbToSend;
    
    mutex.lock();
    nbToSend = this->stocks[ItemType::RehabPatient];
    mutex.unlock();

    // on envoie les patients à l'hôpital en réhabilitation
    const int accepted = this->hospitals[0]->transfer(ItemType::RehabPatient, nbToSend);

    // Si l'hôpital n'a rien accepté, on sort d'ici
    if (accepted <= 0) return;

    // les patients ne sont plus dans la clinique
    mutex.lock();
    this->stocks[ItemType::RehabPatient] -= accepted;
    mutex.unlock();

    // on crée la facture
    this->insurance->invoice(getCostPerService(ServiceType::Treatment) * accepted, this);
}

void Clinic::orderResources() {
    // TODO

    // Pour chaque ressource dont la clinique a besoin
    for (ItemType item : resourcesNeeded) {

        // On cherche un fournisseur compatible
        Supplier* chosenSupplier = nullptr;

        // protéger l'accès concurrent aux données internes de la clinique
        mutex.lock();
        this->stocks[item] += 1;

        for (Seller* sup : this->suppliers) {
            Supplier* supplier = dynamic_cast<Supplier*>(sup);

            if (supplier == nullptr)
                continue;

            if (supplier->sellsResource(item)) {
                chosenSupplier = supplier;

                // On enregistre la facture impayée
                this->unpaidBills.push_back({supplier, getCostPerUnit(item)});
                break;
            }
        }
        mutex.unlock();  // On a fini d'accéder aux données de la clinique

        // On contacte le fournisseur en dehors du verrou
        if (chosenSupplier != nullptr) {
            chosenSupplier->buy(item, 1);
        }
    }
}

void Clinic::treatOne() {
    // TODO

    mutex.lock();

    // pas de patients à traiter
    if (stocks[ItemType::SickPatient] <= 0) {
        mutex.unlock();
        return;
    }

    // si l'on n'a pas assez de fonds, le traitement ne sera pas effectif
    if (!hasResourcesForTreatment()) {
        mutex.unlock();
        return;
    }

    // on consomme les ressources nécessaires
    for (const auto& item : resourcesNeeded) {
        // si la ressource est épuisée, on commande
        if (this->stocks[item] <= 0) {
            mutex.unlock();
            orderResources();
            mutex.lock();
        }
        this->stocks[item] -= 1;
    }

    // on traite un patient
    this->stocks[ItemType::SickPatient] -= 1;
    this->stocks[ItemType::RehabPatient] += 1;

    // on paie le spécialiste
    this->money -= getEmployeeSalary(EmployeeType::TreatmentSpecialist);
    this->nbEmployeesPaid += 1;

    mutex.unlock();
}

void Clinic::pay(int bill) {
    // TODO
    // protéger l'accès concurrent à la liste des factures impayées
    mutex.lock();

    // on ajoute la facture au fonds de la clinique
    this->money += bill;

    // libérer le mutex
    mutex.unlock();
}

Supplier *Clinic::chooseRandomSupplier(ItemType item) {
    std::vector<Supplier*> availableSuppliers;

    // Sélectionner les Suppliers qui ont la ressource recherchée
    for (Seller* seller : suppliers) {
        auto* sup = dynamic_cast<Supplier*>(seller);
        if (sup->sellsResource(item)) {
            availableSuppliers.push_back(sup);
        }
    }

    // Choisir aléatoirement un Supplier dans la liste
    assert(availableSuppliers.size());
    std::vector<Supplier*> out;
    std::sample(availableSuppliers.begin(), availableSuppliers.end(), std::back_inserter(out),
            1, std::mt19937{std::random_device{}()});
    return out.front();
}

void Clinic::setHospitalsAndSuppliers(std::vector<Seller*> hospitals, std::vector<Seller*> suppliers) {
    this->hospitals = hospitals;
    this->suppliers = suppliers;
}

void Clinic::setInsurance(Seller* ins) { 
    insurance = ins; 
}


int Clinic::getTreatmentCost() {
    return 0;
}

int Clinic::getWaitingPatients() {
    return stocks[ItemType::SickPatient];
}

int Clinic::getNumberPatients() {
    return stocks[ItemType::SickPatient] + stocks[ItemType::RehabPatient];
}

Pulmonology::Pulmonology(int uniqueId, int fund) :
    Clinic::Clinic(uniqueId, fund, {ItemType::Pill, ItemType::Thermometer}) {}

Cardiology::Cardiology(int uniqueId, int fund) :
    Clinic::Clinic(uniqueId, fund, {ItemType::Syringe, ItemType::Stethoscope}) {}

Neurology::Neurology(int uniqueId, int fund) :
    Clinic::Clinic(uniqueId, fund, {ItemType::Pill, ItemType::Scalpel}) {}
