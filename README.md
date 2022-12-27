# A Disruption Resilient Transport Protocol (DRTP) for Synchrophasors Measurement in Transmission Grids

 *********************************************************************************
This work is licensed under CC BY-NC-SA 4.0
(https://creativecommons.org/licenses/by-nc-sa/4.0/).

Copyright (c) 2021-2022 zhouby-zjl @ github

This file is a part of "Disruption Resilient Transport Protocol"
(https://github.com/zhouby-zjl/drtp/).

Written by zhouby-zjl @ github

This software is protected by the patents as well as the software copyrights.
 **********************************************************************************
 

## What is the DRTP? 
In a modern transmission grid, the phasor measurement unit requires a reliable transport for its sampled statistics with an end-to-end failure rate (EEFR) as lower as possible to ensure the grid estimation accuracy. However, the EEFR can be deteroriated by packet losses caused by the multiple links disruptions for the primary forwarding path (PP). To address that, the DRTP enables the hop-by-hop retransmissions utilizing the plentiful redundant sub-paths (RSPs) available for the PP to increase reliability. The DRTP can take advantage in keeping the EEFR in 0% and is with the bounded end-to-end delivery time under serious links disruptions for a typical case constructed according to a real transmission grid topology.

## What is the status of the DRTP?
The DRTP is currently in the prototype stage which is comprehensively tested under the IEEE 300 bus dataset and South Carolina 500-bus dataset with an sigfinicantly reduced EEFR performance. We are looking forward to implement into a real router in the future.

## How do I run the source code?
1. You need to download both the DRTP from github and the recent ndnSIM from https://ndnsim.net/current/. 
2. Simply copy all files of the DRTP to the ndnSIM folder in an override manner. 
3. You need to edit the drtp-config-static.ini file under the ndnSIM/ns-3 folder with the correct IEEE bus dataset path, the output log path and some simulation parameters. 
4. Run DRTP testers. First, cd ndnSIM/ns-3. Second, there are two types of tester with the route algorithms available below:
   - a Dijkstra based algorithm to generate multi-path subgraph (GenMPSG). You can run: ./waf --run scratch/drtp-sim-static-dijkstra --command-template="%s drtp-config-dijkstra.ini". 
   - An optimized resilient routes generation (RRG) algorithm. You can run:  ./waf --run scratch/drtp-sim-static --command-template="%s drtp-config-static.ini". 
5. Afterwards, you can find the simulation results under the SIM_LOG_DIR directory defined in the above ini file.

## Publications
[1] Boyang Zhou, Chunming Wu, Qiang Yang and Xiang Chen, "DRTP: A Disruption Resilient Hop-by-Hop Transport Protocol for Synchrophasors Measurement in Electric Transmission Grids," in IEEE Access, doi: 10.1109/ACCESS.2022.3232557. (download: https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=9999427)

[2] Boyang Zhou. Reliable Resilient Router for Wide-Area Phasor Measurement System for Power Grid. PCT DF214063US. 2022. (issued)

[3] Boyang Zhou. Resilient Route Generation System for Reliable Communication in Power Grid Phasor Measurement System. DF220483US. 2022. (publication)

 **********************************************************************************
We are looking forward to new project opportunity in making the DRTP growing up. 
