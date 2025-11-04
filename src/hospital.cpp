// hospital.cpp
#include "hospital.h"
#include "costs.h"
#include <pcosynchro/pcothread.h>

Hospital::Hospital(int id, int fund, int maxBeds)
: Seller(fund, id), maxBeds(maxBeds), nbNursingStaff(maxBeds) {
    stocks[ItemType::SickPatient] = 0;
    stocks[ItemType::RehabPatient] = 0;
}

void Hospital::run() {
    logger() << "Hospital " <<  uniqueId << " starting with fund " << money << ", maxBeds " << maxBeds << std::endl;

    while (true) {
        clock->worker_wait_day_start();

        // TODO condition d'arrêt
        if (PcoThread::thisThread()->stopRequested()) break;

        transferSickPatientsToClinic();
        updateRehab();
        payNursingStaff();

        clock->worker_end_day();
    }

    logger() << "Hospital " <<  uniqueId << " stopping with fund " << money << std::endl;
}

void Hospital::transferSickPatientsToClinic() {
    // TODO
    mutex.lock();

    // Refuse si pas de cliniques associées
    if (clinics.empty()) {
        mutex.unlock();
        return;
    }

    // Refuse si on n'a pas de patients à transférer
    if (stocks[ItemType::SickPatient] <= 0) {
        mutex.unlock();
        return;
    }

    // Refuse si l'hôpital n’a plus de fonds
    if (money <= 0) {
        mutex.unlock();
        return;
    }

    int toSend = stocks[ItemType::SickPatient];
    mutex.unlock();

    // on envoie les patients à la clinique se faire soigner
    int accepted = clinics[0]->transfer(ItemType::SickPatient, toSend);

    // Si la clinique n'a rien accepté, on sort d'ici
    if(accepted <= 0) return;

    // les patients ne sont plus dans l'hôpital
    mutex.lock();
    stocks[ItemType::SickPatient] -= accepted;
    mutex.unlock();

    // Facturation
    insurance->invoice(getCostPerService(ServiceType::PreTreatmentStay) * accepted, this);
}

void Hospital::updateRehab() {
    // TODO

    // Refuse si on n'a pas de patients
    if (stocks[ItemType::RehabPatient] <= 0) {
        return;
    }

    // on décrémente le nombre de jours restants pour chaque patient
    removeOneDayForAllPatients();

    // après 5 jours, un patient en réhabilitation est guéri et libère un lit
    int removedPatients = removePatientsWhenRehabFinished();
    stocks[ItemType::RehabPatient] -= removedPatients;

    // On raque comme d'habitude
    insurance->invoice(getCostPerService(ServiceType::Rehab) * removedPatients, this);
}

void Hospital::removeOneDayForAllPatients() {
    // si pas de patients, on fait rien
    if (this->patientsInfo.empty())
        return;

    // on décrémente le nombre de jours restants pour chaque patient
    for (auto& patientInfo : this->patientsInfo)
        --patientInfo.remainingDays;
}

void Hospital::payNursingStaff() {
    // TODO
    mutex.lock();

    this->money -= getEmployeeSalary(EmployeeType::NursingStaff) * this->nbNursingStaff;
    this->nbEmployeesPaid += this->nbNursingStaff;

    mutex.unlock();
}

void Hospital::pay(int bill) {
    // TODO
    // protéger l'accès concurrent à la liste des factures impayées
    mutex.lock();

    // on ajoute la facture au fonds de l'hôpital
    this->money += bill;

    // libérer le mutex
    mutex.unlock();
}

void Hospital::addPatients(int nbPatients) {
    patientsInfo.push_back({nbPatients, 5});
}

int Hospital::removePatientsWhenRehabFinished() {
    mutex.lock(); 

    auto itRemove = std::remove_if(
        patientsInfo.begin(),
        patientsInfo.end(),
        [](const PatientInfo& p) { return p.remainingDays == 0; }
    );
    
    int removed = 0;

    // on récupère le nombre de patients à retirer
    for (;itRemove != patientsInfo.end(); itRemove++)
        removed += itRemove->nbPatients;

    // on supprime les patients guéris
    patientsInfo.erase(itRemove, patientsInfo.end());

    // on les ajoutes aux patients libérés
    nbFreed += removed;

    mutex.unlock(); 

    return removed;
}

int Hospital::transfer(ItemType what, int qty) {
    // TODO 

    // protége l'accès car la fonction peut être appeler par hopitaux et clinique
    mutex.lock();

    // 1. L'hôpital n'accepte que des patients malades ou en réhabilitation
    if (what != ItemType::SickPatient && what != ItemType::RehabPatient)
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

    // 3. Transferer seulement si des lits sont disponibles
    int availableBeds = maxBeds - getNumberPatients();

    // pas de lits disponibles
    if(availableBeds <= 0)
    {
        mutex.unlock();
        return 0;
    }

    // pas assez de lits disponibles pour tous les patients
    if (availableBeds < qty)
    {
        // on accepte seulement le nombre de patients pouvant être hébergés
        stocks[what] += availableBeds;

        // ajouter les patients en réhabilitation pour tracker le nombre de jours restants
        if (what == ItemType::RehabPatient)
            this->addPatients(availableBeds);

        mutex.unlock();
        return availableBeds;
    }

    // 4. Sinon accepter et ajouter les patients au stock
    stocks[what] += qty;

    // ajouter les patients en réhabilitation pour tracker le nombre de jours restants
    if (what == ItemType::RehabPatient)
        this->addPatients(qty);

    mutex.unlock();
    return qty;
}

int Hospital::getNumberPatients() {
    return stocks[ItemType::SickPatient] + stocks[ItemType::RehabPatient] + nbFreed;
}

void Hospital::setClinics(std::vector<Seller*> c) {
    clinics = std::move(c);
}

void Hospital::setInsurance(Seller* ins) { 
    insurance = ins; 
}
