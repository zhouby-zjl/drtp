# A Disruption Resilient Transport Protocol (DRTP) for Synchrophasors Measurement in Transmission Grids

 *********************************************************************************
This work is licensed under CC BY-NC-SA 4.0
(https://creativecommons.org/licenses/by-nc-sa/4.0/).

Copyright (c) 2021 Boyang Zhou

This file is a part of "Disruption Resilient Transport Protocol"
(https://github.com/zhouby-zjl/drtp/).

Written by Boyang Zhou (zhouby@zhejianglab.com)

This software is protected by the patents numbered with PCT/CN2021/075891,
ZL202110344405.7 and ZL202110144836.9, as well as the software copyrights
numbered with 2020SR1875227 and 2020SR1875228.
 **********************************************************************************
 
 This software is the source code of the DRTP for the manuscript entitled with "A Disruption Resilient NDN Transport Protocol for Synchrophasors Measurement in Transmission Grids" that has been currently submitted to the IEEE Internet-of-Things Journal.
 
## What is the DRTP? 
The DRTP enables a resilient in-network retransmission to satisfy the high reliability and stringent realtimeness in the PMU data collection using protocols like in the IEEE 37.118.2 standard. It can perfectly sustain serious link disruptions with the 0\% end-to-end packet delivery failure rate and stringent low end-to-end packet delivery time.

## What is the status of the DRTP?
The DRTP is currently in the prototype stage which is comprehensively tested under the IEEE 300 bus dataset with an extraordinary performance. We are looking forward to implement into a real router in the future.

## How do I run the source code?
1. You need to download both the DRTP from github and the recent ndnSIM from https://ndnsim.net/current/. 
2. Simply copy all files of the DRTP to the ndnSIM folder in an override manner. 
3. You need to edit the drtp-config-static.ini file under the ndnSIM/ns-3 folder with the correct IEEE bus dataset path, the output log path and some simulation parameters. 
4. cd ndnSIM/ns-3 && ./waf --run scratch/drtp-sim-static --command-template="%s drtp-config-static.ini". 
5. Afterwards, you can find the simulation results under the SIM_LOG_DIR directory defined in the drtp-config-static.ini file.

====================================================================================

We are looking forward to new project opportunity in making the DRTP growing up. 
