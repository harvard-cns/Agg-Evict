### CAIDA traces

We use a one-minute trace from Equinix data center at Chicago from [CAIDA in 2016](http://www.caida.org/data/passive/passive_2016_dataset.xml) with 28.82M TCP and UDP packets. 
We process this raw trace into our tested traces [traffic_sender.dat](https://www.dropbox.com/s/8mrouyhgbn3y715/traffic_sender.dat?dl=0) following this format: 

```
src_ip dst_ip src_port dst_port
src_ip dst_ip src_port dst_port
src_ip dst_ip src_port dst_port
src_ip dst_ip src_port dst_port
...
...
``` 

### Synthetic traces following zipf distribution

We use zipf\_trace\_generator.cpp to generate zipf traces as following:

```
g++ -o zipf_trace_generator zipf_trace_generator.cpp -std=c++11
./zipf_trace_generator

```
This cpp file will generate 11 traces with different skewness. 
You can change the value of *skew* (e.g., setting it to 1.1) in this cpp file to generate zipf traces with specific skewness. 