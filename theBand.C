TGraphAsymmErrors *theBand(TFile *file, int doSyst, int whichChannel, BandType type, double width=0.68) {
    if (file == 0) return 0;
    TTree *t = (TTree *) file->Get("limit");
    if (t == 0) t = (TTree *) file->Get("test"); // backwards compatibility
    if (t == 0) { std::cerr << "TFile " << file->GetName() << " does not contain the tree" << std::endl; return 0; }
    Double_t mass, limit, limitErr = 0; Float_t t_cpu, t_real; Int_t syst, iChannel, iToy, iMass; Float_t quant = -1;
    t->SetBranchAddress("mh", &mass);
    t->SetBranchAddress("limit", &limit);
    if (t->GetBranch("limitErr")) t->SetBranchAddress("limitErr", &limitErr);
    if (t->GetBranch("t_cpu") != 0) {
        t->SetBranchAddress("t_cpu", &t_cpu);
        t->SetBranchAddress("t_real", &t_real);
    }
    if (use_precomputed_quantiles) {
        if (t->GetBranch("quantileExpected") == 0) { std::cerr << "TFile " << file->GetName() << " does not have precomputed quantiles" << std::endl; return 0; }
        t->SetBranchAddress("quantileExpected", &quant);
    }
    t->SetBranchAddress("syst", &syst);
    t->SetBranchAddress((seed_is_channel ? "iSeed" : "iChannel"), &iChannel);
    t->SetBranchAddress("iToy", &iToy);

    std::map<int,std::vector<double> >  dataset;
    std::map<int,std::vector<double> >  errors;
    std::map<int,double>                obsValues;
    for (size_t i = 0, n = t->GetEntries(); i < n; ++i) {
        t->GetEntry(i);
        iMass = int(mass*100);
        //printf("%6d mh=%.1f  limit=%8.3f +/- %8.3f toy=%5d quant=% .3f\n", i, mass, limit, limitErr, iToy, quant);
        if (syst != doSyst)           continue;
        if (iChannel != whichChannel) continue;
        if      (type == Asimov)   { if (iToy != -1) continue; }
        else if (type == Observed) { if (iToy !=  0) continue; }
        else if (type == ObsQuantile && iToy == 0) { obsValues[iMass] = limit; continue; }
        else if (iToy <= 0 && !use_precomputed_quantiles) continue;
        if (limit == 0 && !zero_is_valid) continue; 
        if (type == MeanCPUTime) { 
            if (limit < 0) continue; 
            limit = t_cpu; 
        }
        if (type == MeanRealTime) { 
            if (limit < 0) continue; 
            limit = t_real; 
        }
        if (use_precomputed_quantiles) {
            if (type == CountToys)   return 0;
            if (type == Mean)        return 0;
            //std::cout << "Quantiles. What should I do " << (type == Observed ? " obs" : " exp") << std::endl;
            if (type == Observed && quant > 0) continue;
            if (type == Median) {
                if (fabs(quant - 0.5) > 0.005 && fabs(quant - (1-width)/2) > 0.005 && fabs(quant - (1+width)/2) > 0.005) {
                    //std::cout << " don't care about " << quant << std::endl;
                    continue;
                } else {
                    //std::cout << " will use " << quant << std::endl;
                }
            }
        }
        dataset[iMass].push_back(limit);
        errors[iMass].push_back(limitErr);
    }
    TGraphAsymmErrors *tge = new TGraphAsymmErrors(); 
    int ip = 0;
    for (std::map<int,std::vector<double> >::iterator it = dataset.begin(), ed = dataset.end(); it != ed; ++it) {
        std::vector<double> &data = it->second;
        int nd = data.size();
        std::sort(data.begin(), data.end());
        double median = (data.size() % 2 == 0 ? 0.5*(data[nd/2]+data[nd/2+1]) : data[nd/2]);
        if (band_safety_crop > 0) {
            std::vector<double> data2;
            for (int j = 0; j < nd; ++j) {
                if (data[j] > median*band_safety_crop && data[j] < median/band_safety_crop) {
                    data2.push_back(data[j]);
                }
            }
            data2.swap(data);
            nd = data.size();
            median = (data.size() % 2 == 0 ? 0.5*(data[nd/2]+data[nd/2+1]) : data[nd/2]);
        }
        double mean = 0; for (int j = 0; j < nd; ++j) mean += data[j]; mean /= nd;
        double summer68 = data[floor(nd * 0.5*(1-width)+0.5)], winter68 =  data[std::min(int(floor(nd * 0.5*(1+width)+0.5)), nd-1)];
        if (use_precomputed_quantiles && type == Median) {
            if (precomputed_median_only && data.size() == 1) {
                mean = median = summer68 = winter68 = data[0];
            } else if (data.size() != 3) { 
                std::cerr << "Error for expected quantile for mass " << it->first << ": size of data is " << data.size() << std::endl; 
                continue; 
            } else {
                mean = median = data[1]; summer68 = data[0]; winter68 = data[2];
            }
        }
        double x = mean;
        switch (type) {
            case MeanCPUTime:
            case MeanRealTime:
            case Mean: x = mean; break;
            case Median: x = median; break;
            case CountToys: x = summer68 = winter68 = nd; break;
            case Asimov: // mean (in case we did it more than once), with no band
                x = summer68 = winter68 = (obs_avg_mode == mean ? mean : median);
                break;
            case Observed:
                x = mean;
                if (nd == 1) {
                    if (errors[it->first].size() == 1) {
                        summer68 = mean - errors[it->first][0];
                        winter68 = mean + errors[it->first][0];
                    } else {
                        // could happen if limitErr is not available
                        summer68 = winter68 = mean;
                    }
                } else { // if we have multiple, average and report rms (useful e.g. for MCMC)
                    switch (obs_avg_mode) {
                        case MeanObs:   x = mean; break;
                        case MedianObs: x = median; break;
                        case LogMeanObs: {
                                 x = 0;
                                 for (int j = 0; j < nd; ++j) { x += log(data[j]); }
                                  x = exp(x/nd);
                             } 
                             break;
                    }
                    double rms = 0;
                    for (int j = 0; j < nd; ++j) { rms += (x-data[j])*(x-data[j]); }
                    rms = sqrt(rms/(nd*(nd-1)));
                    summer68 = mean - rms;
                    winter68 = mean + rms;
                }
                break;
            case AdHoc:
                x = summer68 = winter68 = mean;
                break;
            case Quantile: // get the quantile equal to width, and it's uncertainty
                x = data[floor(nd*width+0.5)];
                summer68 = x - quantErr(nd, &data[0], width);
                winter68 = x + (x-summer68);
                break;
            case ObsQuantile:
                {   
                    if (obsValues.find(it->first) == obsValues.end()) continue;
                    int pass = 0, fail = 0;
                    for (int i = 0; i < nd && data[i] <= obsValues[it->first]; ++i) {
                        fail++;
                    }
                    pass = nd - fail; x = double(pass)/nd;
                    double alpha = (1.0 - .68540158589942957)/2;
                    summer68 = (pass == 0) ? 0.0 : ROOT::Math::beta_quantile(   alpha, pass,   fail+1 );
                    winter68 = (fail == 0) ? 1.0 : ROOT::Math::beta_quantile( 1-alpha, pass+1, fail   );
                    break;
                }
        } // end switch
        tge->Set(ip+1);
        tge->SetPoint(ip, it->first*0.01, x);
        tge->SetPointError(ip, 0, 0, x-summer68, winter68-x);
        ip++;
    }
    return tge;
}
