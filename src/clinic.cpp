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

    while (!PcoThread::thisThread()->stopRequested()) {
        clock->worker_wait_day_start();
        // TODO condition d'arrêt
        if (false) break;

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
    // 1. La clinique n'accepte que des patients malades
    if (what != ItemType::SickPatient)
        return 0;

    // 2. Refuse si la clinique n’a plus de fonds    
    if (money <= 0)
        return 0;

    // 3. Refuser si elle a des factures impayées
    if (this->unpaidBills.size() > 0)
        return 0;   

    // 4. Sinon accepter et ajouter les patients au stock
    stocks[what] += qty;
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
    
    for (std::pair<Supplier*, int>& bill : this->unpaidBills)
    {
        // on paie la facture au Supplier
        bill.first->pay(bill.second);

        // on retire le montant de la facture des fonds de la clinique
        this->money -= bill.second;
    }
        
    // on retire les factures de la liste des factures impayées    
    this->unpaidBills.clear();
}

void Clinic::processNextPatient() {
    // TODO
    treatOne();
}

void Clinic::sendPatientsToRehab() {
    // TODO
    // on envoie les patients à l'hôpital de réhabilitation
    this->hospitals[0]->transfer(ItemType::RehabPatient, this->stocks[ItemType::RehabPatient]);

    // on reçoit l'argent du transfert
    this->money += getCostPerService(ServiceType::Treatment) * this->stocks[ItemType::RehabPatient];

    // les patients ne sont plus dans la clinique
    this->stocks[ItemType::RehabPatient] = 0;
}

void Clinic::orderResources() {
    // TODO    
    for (ItemType item : resourcesNeeded) {
        // On reçoit une ressource commandée
        this->stocks[item] += 1;

        // on retire du supplier la ressource commandée
        for (Seller* sup : this->suppliers)
        {
            Supplier* supplier = dynamic_cast<Supplier*>(sup);
            
            // verification si le Seller n'est pas un Supplier on passe au suivant
            if (supplier == nullptr)
                continue;          

            // Si le Supplier vend la ressource, on effectue le transfert
            if (supplier->sellsResource(item)) {
                supplier->buy(item, 1);

                // on ajoute la facture impayée
                this->unpaidBills.push_back({supplier, getCostPerUnit(item)});
                break;
            }
        } 
    }
}

void Clinic::treatOne() {
    // TODO
    // si l'on n'a pas assez de fonds, le traitement ne sera pas effectif
    if(!hasResourcesForTreatment())
        return;

    // on consomme les ressources nécessaires
    for (const auto& item : resourcesNeeded) {
        
        // si la ressource est épuisée, on commande
        if (this->stocks[item] <= 0)
            orderResources();
        
        this->stocks[item] -= 1;
    }

    // on traite un patient
    this->stocks[ItemType::SickPatient] -= 1;
    this->stocks[ItemType::RehabPatient] += 1;

    // on paie le spécialiste
    this->money -= getEmployeeSalary(EmployeeType::TreatmentSpecialist);
    this->nbEmployeesPaid += 1;
}

void Clinic::pay(int bill) {
    // TODO
    this->money += bill;
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
