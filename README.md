# A Disruption Resilient Transport Protocol (DRTP) for Synchrophasors Measurement in Transmission Grids

## What is the DRTP? 
In a modern electric power transmission grid, the phasor measurement unit requires a reliable transport of its sampled statistics with a low end-to-end failure rate (EEFR) to ensure the accuracy of the grid state estimation. However, EEFR can be deteriorated by packet losses due to multiple link disruptions in the primary forwarding path (PP). To address that, we investigate a novel disruption resilient transport protocol (DRTP) enabling hop-by-hop retransmission utilizing the redundant subpaths (RSPs) available for the PP to increase reliability. It addresses the new distributed collaboration issue under multiple link failures to avoid cache mismatching. These have not been considered by the existing approaches. The DRTP was evaluated in the ndnSIM simulator through both the typical and general routes that are constructed from real transmission grids. The numerical results demonstrate that it has a significant advantage in reducing the EEFR with a low end-to-end delivery time under serious link disruptions.

## What is the status of the DRTP?
The DRTP is currently in the prototype stage which is comprehensively tested under the IEEE 300 bus dataset and South Carolina 500-bus dataset with an sigfinicantly reduced EEFR performance. We are looking forward to implement into a real router in the future.

## How do I run the source code?
1. You need to download both the DRTP from github and the recent ndnSIM from https://ndnsim.net/current/. 
2. Simply copy all files of the DRTP to the ndnSIM folder in an override manner. 
3. You need to edit the drtp-config-static.ini file under the ndnSIM/ns-3 folder with the correct IEEE bus dataset path, the output log path and some simulation parameters. 
4. Run DRTP testers. First, cd ndnSIM/ns-3. Second, there are the tester with the route algorithms available below:
   - A Dijkstra based algorithm to generate multi-path subgraph (GenMPSG). You can run: ./waf --run scratch/drtp-sim-static-dijkstra --command-template="%s drtp-config-dijkstra.ini". 
5. Afterwards, you can find the simulation results under the SIM_LOG_DIR directory defined in the above ini file.

## Other Available MPSG Generation Algorithm 
Compared to GenMPSG, a new Heuristic MPSG Generation Algorithm (HeuMPSG) can improve the DRTP resilience, which is available in https://github.com/zhouby-zjl/heumpsg/

 ## Publications
**[1] Boyang Zhou, Chunming Wu, Qiang Yang and Xiang Chen, "DRTP: A Disruption Resilient Hop-by-Hop Transport Protocol for Synchrophasors Measurement in Electric Transmission Grids," in IEEE Access, vol. 10, pp. 133898-133914, 2022, doi: 10.1109/ACCESS.2022.3232557. (download: https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=9999427)**

[2] Boyang Zhou. Reliable Resilient Router for Wide-Area Phasor Measurement System for Power Grid. PCT DF214063US. 2022. (issued)

[3] Boyang Zhou. Resilient Route Generation System for Reliable Communication in Power Grid Phasor Measurement System. DF220483US. 2022. (publication)


 *********************************************************************************
This work is licensed under CC BY-NC-SA 4.0
(https://creativecommons.org/licenses/by-nc-sa/4.0/).

Copyright (c) 2021-2023 Boyang Zhou @ Zhejiang Lab

This file is a part of "Disruption Resilient Transport Protocol"
(https://github.com/zhouby-zjl/drtp/).

This software is protected by the patents as well as the software copyrights.
 **********************************************************************************


 **********************************************************************************
We are looking forward to new project opportunity in making the DRTP growing up. 
