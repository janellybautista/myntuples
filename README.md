# 1. Produce Ntuple from DUNE FD MC files 

The following instruction is used to produce ROOT n-tuples from FD MC CAF files (mcc11): [FD Beamsim Requests](https://dune-data.fnal.gov/mc/mcc11/index.html). 

Output: " $\color{#FF0000}{myntuple.root}$ ". 

## First time environment setup only



```
cd /dune/app/users/$USER                                               
mkdir FDEff (first time only)
cd FDEff

source /cvmfs/dune.opensciencegrid.org/products/dune/setup_dune.sh
setup dunetpc v09_22_02 -q e19:debug
[optional if run interactively]:  setup_fnal_security                     # A FNAL grid proxy to submit jobs and access data in dCache via xrootd or ifdh.

mrb newDev
# The prompt ask you to run this:
source /dune/app/users/<your_username>/inspect/localProducts_larsoft_${LARSOFT_VERSION}_debug_${COMPILER}/setup
# For example, mine is: source /dune/app/users/janelly/FDEff/localProducts_larsoft_v09_22_02_debug_e19/setup

cd srcs
git clone https://github.com/weishi10141993/myntuples.git               # First time only, checkout the analysis code from GitHub

mrb uc                                                                  # Tell mrb to update CMakeLists.txt with the latest version numbers of the products.
cd ${MRB_BUILDDIR}                                                      # Go to your build directory
mrb z
mrbsetenv                                                               # Create the bookkeeping files needed to compile programs.
mrb b                                                                   # Compile the code in ${MRB_SOURCE}
```

Produce a ROOT nTuple by running on DUNE FD MC files ($\color{#FF0000}{Output~file}$ : $\color{#FF0000}{myntuple.root}$) 
```
cd /dune/app/users/$USER/FDEff/srcs/myntuples/myntuples/MyEnergyAnalysis
lar -c MyEnergyAnalysis.fcl -n -1                                       # Obtain myntuple.root
# 10k evts take about 32 minutes
```

# Enviroment setup when loging back

```
source /cvmfs/dune.opensciencegrid.org/products/dune/setup_dune.sh
setup dunetpc v09_22_02 -q e19:debug
source /dune/app/users/$USER/FDEff/localProducts_larsoft_v09_22_02_debug_e19/setup
mrbsetenv
cd /dune/app/users/$USER/FDEff/srcs/myntuples/myntuples/MyEnergyAnalysis
```

If added new package in ```srcs``` directory, do ```mrb uc``` and then recompile as above.

To commit changed code changes to remote repository:

```
git commit
git push
```
## Locate files with SAM (Not required but can be helpful)

To get a list of all files in DUNE dataset, use [SAM](https://dune.github.io/computing-training-basics/03-data-management/index.html):

```
source /cvmfs/dune.opensciencegrid.org/products/dune/setup_dune.sh
setup dunetpc v09_22_02 -q e19:debug
setup_fnal_security
setup sam_web_client
export SAM_EXPERIMENT=dune
samweb list-file-locations --dim="defname:prodgenie_nu_dune10kt_1x2x6_mcc11_lbl_reco" --schema=root -e dune
```

Or list 100 files in the dataset, do this at command line:

```
for k in `samweb list-files defname: prodgenie_nu_dune10kt_1x2x6_mcc11_lbl_reco with limit 100`; do samweb get-file-access-url --schema root $k; done
```

You can also find 'dimensions' of the sample from [MCC11](https://dune-data.fnal.gov/mc/mcc11/index.html). For example, the following is for the FD nutau reco sample

```
samweb list-files "file_type mc and dune.campaign mcc11 and data_tier 'full-reconstructed' and application reco and version v07_06_02 and file_name nutau_%"
```

This returns a list of files saved here: ```/dune/app/users/weishi/MCC11FDBeamsim_nutau_reco.txt```.

Then to locate full directory of each file:
```
samweb get-file-access-url nutau_dune10kt_1x2x6_12855916_0_20181104T221500_gen_g4_detsim_reco.root --schema=root
```
It returns the full url, e.g.,
```
root://fndca1.fnal.gov:1094/pnfs/fnal.gov/usr/dune/tape_backed/dunepro/mcc11/protodune/mc/full-reconstructed/08/65/76/12/nutau_dune10kt_1x2x6_12855916_0_20181104T221500_gen_g4_detsim_reco.root
```

You can run ```lar``` directly on the url or open it via ROOT (no need to copy it):
```
lar -c <your fcl file>.fcl -n -1 root://fndca1.fnal.gov:1094/pnfs/fnal.gov/usr/dune/tape_backed/dunepro/mcc11/protodune/mc/full-reconstructed/08/65/76/12/nutau_dune10kt_1x2x6_12855916_0_20181104T221500_gen_g4_detsim_reco.root

root -l root://fndca1.fnal.gov:1094/pnfs/fnal.gov/usr/dune/tape_backed/dunepro/mcc11/protodune/mc/full-reconstructed/08/65/76/12/nutau_dune10kt_1x2x6_12855916_0_20181104T221500_gen_g4_detsim_reco.root
```

Another way to locate file:
```
samweb locate-file nutau_dune10kt_1x2x6_12855916_0_20181104T221500_gen_g4_detsim_reco.root
```
It tells you all the locations, e.g.,
```
enstore:/pnfs/dune/tape_backed/dunepro/mcc11/protodune/mc/full-reconstructed/08/65/76/12(14064@vr0423m8)
```

Other SAM functions: https://wiki.dunescience.org/wiki/DUNE_Computing/Main_resources_Jan2021#Data_management:_best_practices

```
source /cvmfs/dune.opensciencegrid.org/products/dune/setup_dune.sh
setup jobsub_client

# Need to prestage files from tape
samweb prestage-dataset --defname='prodgenie_nu_dune10kt_1x2x6_mcc11_lbl_reco' --parallel=6
```

Some samples are on tape, you can't access unless you prestage it first (may take time): https://dune.github.io/computing-training-basics/07-grid-job-submission/index.html

```
samweb prestage-dataset --defname='protodune-sp_runse6201_reco_v09_09_01_v0'
```

A better way to prestage is to instead do
```
unsetup curl # necessary as of May 2022 because there's an odd interaction with the UPS version of curl, so we need to turn it off
samweb run-project --defname=kherner-may2022tutorial-mc --schema https 'echo %fileurl && curl -L --cert $X509_USER_PROXY --key $X509_USER_PROXY --cacert $X509_USER_PROXY --capath /etc/grid-security/certificates -H "Range: bytes=0-3" %fileurl && echo'
```
