// hospital.cpp
#include "hospital.h"
#include "costs.h"
#include <pcosynchro/pcothread.h>

Hospital::Hospital(int id, int fund, int maxBeds)
: Seller(fund, id), maxBeds(maxBeds), nbNursingStaff(maxBeds) {
    stocks[ItemType::SickPatient] = 0;
    stocks[ItemType::RehabPatient] = 0;
}

static PcoMutex mutex;

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
    // pas de cliniques associées
    if (clinics.empty()) return;

    // pas de patients à transférer
    if (stocks[ItemType::SickPatient] <= 0) return;

    // Refuse si l'hôpital n’a plus de fonds
    if (money <= 0)
        return;    

    // on envoie les patients à la clinique se faire soigner
    int accepted = this->clinics[0]->transfer(ItemType::SickPatient, this->stocks[ItemType::SickPatient]);

    // Si la clinique n'a rien accepté, on sort d'ici
    if(accepted <= 0) return;

    // les patients ne sont plus dans l'hôpital
    this->stocks[ItemType::SickPatient] -= accepted;

    // on reçoit l'argent du transfert
    this->insurance->invoice(getCostPerService(ServiceType::PreTreatmentStay) * accepted, this);
}

void Hospital::updateRehab() {
    // TODO
    if (stocks[ItemType::RehabPatient] <= 0) return;

    // on décrémente le nombre de jours restants pour chaque patient
    removeOneDayForAllPatients();

    // après 5 jours, un patient en réhabilitation est guéri et libère un lit
    int removedPatients = removePatientsWhenRehabFinished();
    this->stocks[ItemType::RehabPatient] -= removedPatients;

    // On raque comme d'habitude
    this->money += getCostPerService(ServiceType::Rehab) * removedPatients;
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
    this->money -= getEmployeeSalary(EmployeeType::NursingStaff) * this->nbNursingStaff;
    this->nbEmployeesPaid += this->nbNursingStaff;
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
    this->patientsInfo.push_back({nbPatients, 5});
}

int Hospital::removePatientsWhenRehabFinished() {
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

    return removed;
}

int Hospital::transfer(ItemType what, int qty) {
    // TODO 
    // 1. L'hôpital n'accepte que des patients malades ou en réhabilitation
    if (what != ItemType::SickPatient && what != ItemType::RehabPatient)
        return 0;

    // 2. Refuse si la clinique n’a plus de fonds    
    if (money <= 0)
        return 0;

    // 3. Transferer seulement si des lits sont disponibles
    int availableBeds = maxBeds - getNumberPatients();

    // pas de lits disponibles
    if(availableBeds <= 0)
        return 0;

    // pas assez de lits disponibles pour tous les patients
    if (availableBeds < qty)
    {
        // on accepte seulement le nombre de patients pouvant être hébergés
        stocks[what] += availableBeds;

        // ajouter les patients en réhabilitation pour tracker le nombre de jours restants
        if (what == ItemType::RehabPatient)
            this->addPatients(availableBeds);

        return availableBeds;
    }

    // 4. Sinon accepter et ajouter les patients au stock
    stocks[what] += qty;

    // ajouter les patients en réhabilitation pour tracker le nombre de jours restants
    if (what == ItemType::RehabPatient)
        this->addPatients(qty);

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
