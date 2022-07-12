# A Disruption Resilient Transport Protocol (DRTP) for Synchrophasors Measurement in Transmission Grids

 *********************************************************************************
This work is licensed under CC BY-NC-SA 4.0
(https://creativecommons.org/licenses/by-nc-sa/4.0/).

Copyright (c) 2021 zhouby-zjl @ github

This file is a part of "Disruption Resilient Transport Protocol"
(https://github.com/zhouby-zjl/drtp/).

Written by zhouby-zjl @ github

This software is protected by the patents as well as the software copyrights.
 **********************************************************************************
 
 This software is the source code of the DRTP for the manuscript entitled with "DRTP: A Disruption Resilient NDN Transport Protocol for Synchrophasors Measurement in Electric Transmission Grids" that has been currently submitted to the IEEE Transactions on Industrial Informatics.
 
## What is the DRTP? 
In a modern transmission grid, the phasor measurement unit requires a reliable transport for its sampled statistics with an end-to-end failure rate (EEFR) as lower as possible to ensure the grid estimation accuracy. However, the EEFR can be deteroriated by packet losses caused by the multiple links disruptions for the primary forwarding path (PP). To address that, the DRTP enables the hop-by-hop retransmissions utilizing the plentiful redundant sub-paths (RSPs) available for the PP to increase reliability. The DRTP can take advantage in keeping the EEFR in 0% and is with the bounded end-to-end delivery time under serious links disruptions for a typical case constructed according to a real transmission grid topology.

## What is the status of the DRTP?
The DRTP is currently in the prototype stage which is comprehensively tested under the IEEE 300 bus dataset with an sigfinicantly reduced EEFR performance. We are looking forward to implement into a real router in the future.

## How do I run the source code?
1. You need to download both the DRTP from github and the recent ndnSIM from https://ndnsim.net/current/. 
2. Simply copy all files of the DRTP to the ndnSIM folder in an override manner. 
3. You need to edit the drtp-config-static.ini file under the ndnSIM/ns-3 folder with the correct IEEE bus dataset path, the output log path and some simulation parameters. 
4. cd ndnSIM/ns-3 && ./waf --run scratch/drtp-sim-static --command-template="%s drtp-config-static.ini". 
5. Afterwards, you can find the simulation results under the SIM_LOG_DIR directory defined in the drtp-config-static.ini file.

 **********************************************************************************

We are looking forward to new project opportunity in making the DRTP growing up. 
