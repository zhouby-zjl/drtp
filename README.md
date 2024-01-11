# The Source Code of the Resilient Sensor Data Dissemination Mechanism (RSDD) Implementing the Disruption Resilient Transport Protocol (DRTP) for Power Transmission Grids

## What are the RSDD and DRTP? 
In today’s power transmission grids, Internet-of-Things networks employ long-haul optical wires for regular sensor data dissemination to a server. Ensuring resilience against link faults is paramount to observe the grid states accurately via a process known as state estimation (SE). The accuracy is achieved by minimizing the end-to-end failure rate in packet delivery (EEFR). Current approaches focus on hop-by-hop retransmission control with in-path caching. Notably, the disruption-resilient transport protocol (DRTP) stands out for achieving the lowest EEFR. DRTP employs robust hop-by-hop retransmission and a recursive collaboration process guided by arrival timeouts. However, challenges arise in maintaining recursiveness with timeouts, leading to increased EEFR due to cache mismatch. These intensify when a hop triggers arrival timeouts, spawning retransmission instances in an unexpected sequence, which can experience an unprotected parallel race condition. To address this, we propose RSDD, a resilient mechanism for sensor data dissemination for implementing DRTP in the correct and fully verified manner. RSDD orchestrates concurrent retransmission instances, ensuring exclusive execution for the same lost packet, precisely scheduled based on timeouts. We evaluated the performance of RSDD in a simulated network that combines SE and a grid, using ndnSIM, MATPOWER, and RTDS. The results validate RSDD as a correct DRTP implementation, highlighting its exclusiveness and quality-of-service performance. RSDD achieves an EEFR of 2.44% and an average end-to-end packet delivery time (EEDT) of 2.7 ms during full path disruption with a 20% link loss rate in packets. Moreover, RSDD excels in enabling SE to maintain the grid observability and accuracy. 

## How do I run the source code?
1. You need to download both the DRTP from github and the recent ndnSIM from https://ndnsim.net/current/. 
2. Simply copy all files of the DRTP to the ndnSIM folder in an override manner. 
3. You need to edit the drtp-config-static.ini file under the ndnSIM/ns-3 folder with the correct IEEE bus dataset path, the output log path and some simulation parameters. 
4. Run DRTP testers. First, cd ndnSIM/ns-3. Second, there are the tester with the route algorithms available below:
   - A Dijkstra based algorithm to generate multi-path subgraph (GenMPSG). You can run: ./waf --run scratch/drtp-sim-static-dijkstra --command-template="%s drtp-config-dijkstra.ini". 
5. Afterwards, you can find the simulation results under the SIM_LOG_DIR directory defined in the above ini file.

 ## Publications
[1] Boyang Zhou, Chunming Wu, Qiang Yang, Yaguan Qian and Yinghui Nie, "Resilient Sensor Data Dissemination to Mitigate Link Faults in IoT Networks with Long-Haul Optical Wires for Power Transmission Grids," in IEEE Internet-of-Things Journal, Jan. 2014. (accepted and to be appear soon. This paper presents RSDD in detail with concrete design in state transition machines and correctness verfication). 
 
[2] Boyang Zhou, Chunming Wu, Qiang Yang and Xiang Chen, "DRTP: A Disruption Resilient Hop-by-Hop Transport Protocol for Synchrophasors Measurement in Electric Transmission Grids," in IEEE Access, vol. 10, pp. 133898-133914, 2022, doi: 10.1109/ACCESS.2022.3232557. (download: https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=9999427)** (This paper deeply introduces DRTP in its concept and collaboration sequences)

[3] Boyang Zhou, Ye Tong, Yinghui Nie, Chunming Wu, Wenjie Yu. Generating Routes Less Fragile to Improve Disruption-Resilient Transport Protocol for Phasor Data in Optical IoT Networks of Power Transmission Grids. IEEE ICPADS 2023. (The paper has been accepted to be presented soon. This paper studies on how to optimize the underlying routes of DRTP for a better resilience.)

[4] Boyang Zhou. Reliable Resilient Router for Wide-Area Phasor Measurement System for Power Grid. PCT DF214063US. 2022. (issued)

[5] Boyang Zhou. Resilient Route Generation System for Reliable Communication in Power Grid Phasor Measurement System. DF220483US. 2022. (publication)


We are looking forward to new project opportunity in making the RSDD and DRTP growing up. 

 *********************************************************************************
This work is licensed under CC BY-NC-SA 4.0
(https://creativecommons.org/licenses/by-nc-sa/4.0/).

Copyright (c) 2021-2024 Boyang Zhou @ Zhejiang Lab

This file is a part of "Resilient Sensor Data Dissemination Mechanism" and "Disruption Resilient Transport Protocol"
(https://github.com/zhouby-zjl/drtp/).

This software is protected by the patents as well as the software copyrights.
 **********************************************************************************
