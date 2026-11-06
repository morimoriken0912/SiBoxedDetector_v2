#include <iostream>
#include <string>
#include <TFile.h>
#include <TTree.h>
#include <TRandom3.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_root_file>" << std::endl;
        return 1;
    }

    std::string inFile = argv[1];
    std::string outFile = inFile;
    size_t pos = outFile.rfind(".root");
    if (pos != std::string::npos) {
        outFile.insert(pos, "_smear");
    } else {
        outFile += "_smear.root";
    }

    TFile* fIn = TFile::Open(inFile.c_str(), "READ");
    if (!fIn || fIn->IsZombie()) {
        std::cerr << "Error: Cannot open " << inFile << std::endl;
        return 1;
    }

    TFile* fOut = new TFile(outFile.c_str(), "RECREATE");

    // 1. eventsツリーのコピー（存在する場合そのまま丸ごとコピー）
    TTree* tEvents = (TTree*)fIn->Get("events");
    if (tEvents) {
        fOut->cd();
        TTree* newEvents = tEvents->CloneTree(-1, "fast");
        newEvents->Write();
    }

    // 2. hitsツリーのスメアリング処理
    TTree* tHits = (TTree*)fIn->Get("hits");
    if (tHits) {
        fOut->cd();
        // ツリーの構造だけをクローン（データはまだ空）
        TTree* newHits = tHits->CloneTree(0);

        int eventID = 0, pixelID = 0, pdg = 0;
        double edep = 0.0, time = 0.0;

        tHits->SetBranchAddress("eventID", &eventID);
        tHits->SetBranchAddress("pixelID", &pixelID);
        tHits->SetBranchAddress("edep", &edep);
        if (tHits->GetBranch("time")) tHits->SetBranchAddress("time", &time);
        if (tHits->GetBranch("pdg")) tHits->SetBranchAddress("pdg", &pdg);

        TRandom3 rand(0); // seed=0は実行のたびに異なる乱数を生成
        const double sigma = 0.030; // 30 keV

        long nEntries = tHits->GetEntries();
        for (long i = 0; i < nEntries; ++i) {
            tHits->GetEntry(i);

            // 0以上の付与エネルギーがある場合のみスメアリング
            if (edep > 0.0) {
                edep = rand.Gaus(edep, sigma);
                if (edep < 0.0) edep = 0.0; // 負のエネルギーを防止
            }

            newHits->Fill();
        }
        newHits->Write();
    }

    fOut->Close();
    fIn->Close();

    std::cout << "[add_smearing] Created smeared file: " << outFile << std::endl;
    return 0;
}