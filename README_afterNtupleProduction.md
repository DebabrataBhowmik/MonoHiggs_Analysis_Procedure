# MonoHiggs_Analysis_Procedure

Once you have the nTuples from the microAOD, you need to get the Xsec of the MC generated

Go to /afs/cern.ch/work/d/dbhowmik/public/Analysis/Data-MC/CMSSW_9_4_6/src/ShilpiDi_suggestions

you can find the commands.txt file there, if the sample is in dbs=global, you can take MINIAOD dataset name and run the command,
but if you produce the samples privately they will be produced in dbs=phys03, in that case you can take one MINIAOD file in this
directory and run the command

cmsRun ana.py inputFiles="file:MINIAODSIM36_1.root" maxEvents=-1

"file:" is of course needed as they are local files

Once you get the xsec go to directory : /afs/cern.ch/work/d/dbhowmik/public/Analysis/MHgg/2017Analysis/CMSSW_9_4_6/src/MonoHiggsToGG/analysis/work/macros

check the path inside myrunAddWeightsAll.sh and run

./myrunAddWeightsAll.sh 41.529(lumi)

./myrunSkimAll.sh

Then go to ../../fits i.e. (analysis/fits) and run

./myformatNtupleForFitting_METcat.sh

