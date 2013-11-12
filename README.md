Chaos
======

Chaos is a versatile, efficient, and reliable primitive for sharing data among all nodes in a low-power wireless network.
Applications of Chaos are diverse and include the computation of aggregate functions, network-wide agreement, atomic broadcast, reliable dissemination, and all-to-all communication.

To start an all-to-all interaction using Chaos, an appointed *initiator* sends a packet containing the data it wants to share will all others (e.g., a temperature reading).
Nodes overhearing the transmission merge their own data with the data received from the initiator and transmit the result (e.g., a packet containing the maximum of their own temperature reading and the received one) synchronously.
Other nodes receive one of the packets due to the *capture effect*, merge data, and transmit again.
This process continues in a distributed and "chaotic" manner until all nodes in the network share the same data (e.g., the maximum temperature across all nodes in the network), which typically  takes less than 100 milliseconds in a 100-node multi-hop network.

The versatility of Chaos stems from the programmable *merge operator* a Chaos user needs to supply.
The merge operator specifies how a node should combine its own with the received data to produce a single output during an all-to-all interaction.
Chaos supports merge operators that take tens of thousands of clock cycles to execute, thus giving users a lot of flexibility and the possibility to perform fairly sophisticated computations.

To learn more about Chaos, check out our [SenSys'13 paper](http://dl.acm.org/citation.cfm?id=2517358).

Code Layout
-----------

`/chaos/contiki/apps/chaos-test` (test application)

`/chaos/contiki/core/dev` (Chaos header and source files)


Building and Running Chaos
----------------------

### Configuration 

There are a few parameters in `/chaos/contiki/apps/chaos-test.h` that you should set according to your needs:

* `#define INITIATOR_NODE_ID 1` (the node ID of the initiator, 1 by default)
* `#define CHAOS_NODES 3` (the total number of nodes in the network, 3 by default)

These two are essential; you can find many more configuration parameters in `/chaos/contiki/apps/chaos-test.h`, `/chaos/contiki/core/dev/chaos.h`, and `/chaos/contiki/core/deploy/testbed.h`. You can either set them in the file directly or feed them as parameters to your compiler.

### Testbed

Here is one example for 15 nodes and node 3 acting as initiator:

`make chaos-test.sky TARGET=sky DEFINES=CHAOS_NODES=15,INITIATOR_NODE_ID=3`

### Cooja simulator 

To run Chaos in Cooja, you have to set `COOJA=1` (0 by default) in your defines (see testbed example above). In addition, you should use the MRM radio model in Cooja. Please note that Chaos pushes Cooja to its limits (and sometimes beyond). As a result, you might see some artifacts when running Chaos in Cooja. However, commonly Chaos and Cooja get along well.

### System setup 

Just for your reference, evalution of this work was done with `gcc version 4.6.3 20120301 (mspgcc LTS 20120406 unpatched) (GCC)`.

Research
--------

Chaos has been developed at the [Department of Computer Science and Engineering](http://www.chalmers.se/cse/EN/) at [Chalmers University of Technology](http://www.chalmers.se/en/Pages/default.aspx) and at the [Computer Engineering and Networks Laboratory](http://www.tec.ethz.ch/) at [ETH Zurich](http://www.ethz.ch/index_EN). The Chaos team consists of [Olaf Landsiedel](http://www.cse.chalmers.se/~olafl/), [Federico Ferrari](http://www.tik.ee.ethz.ch/~ferrarif/), and [Marco Zimmerling](http://www.tik.ee.ethz.ch/~marcoz/). Please contact us for more information.
