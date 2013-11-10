Chaos
======

Chaos is a versatile, efficient, and reliable primitive for sharing data among all nodes in a low-power wireless network.
Applications of Chaos are diverse and include the computation of aggregate functions, network-wide agreement, atomic broadcast, reliable dissemination, and all-to-all communication.

To start an all-to-all interaction using Chaos, an appointed *initiator* sends a packet containing the data it wants to share will all others (e.g., a temperature reading).
Nodes overhearing the transmission merge their own data with the data received from the initiator and transmit the result (e.g., a packet containing the maximum of the local and the received temperature) synchronously.
Other nodes receive one of the packets due to the *capture effect*, merge data, and transmit again.
This process continues until all nodes in the network share the same data (e.g., the maximum temperature across all nodes in the network), which takes typically less than 100 milliseconds in a 100-node multi-hop network.

The versatility of Chaos stems from the programmable *merge operator* a Chaos user needs to supply.
The merge operator specifies how a node should combine its own with the received data to produce a single output during an all-to-all interaction.
Chaos supports merge operators that take tens of thousands of clock cycles to execute, thus giving users a lot of flexibility and the possibility to perform fairly sophisticated computations.

To learn more about Chaos, check out our [SenSys'13 paper](http://dl.acm.org/citation.cfm?id=2517358).

Code Layout
-----------

`/chaos/contiki/apps/` (Chaos example app)

Building and Running Chaos
----------------------

Research
--------

Chaos has been developed at the [Department of Computer Science and Engineering](http://www.chalmers.se/cse/EN/) at [Chalmers University of Technology](http://www.chalmers.se/en/Pages/default.aspx) and at the [Computer Engineering and Networks Laboratory](http://www.tec.ethz.ch/) at [ETH Zurich](http://www.ethz.ch/index_EN). The Chaos team consists of [Olaf Landsiedel](http://www.cse.chalmers.se/~olafl/), [Federico Ferrari](http://www.tik.ee.ethz.ch/~ferrarif/), and [Marco Zimmerling](http://www.tik.ee.ethz.ch/~marcoz/). Please contact us for more information.