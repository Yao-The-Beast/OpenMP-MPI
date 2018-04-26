python WriteSbatch.py -p haswell -r 32 -n 64 -b P2P
python WriteSbatch.py -p haswell -r 32 -n 32 -b Broadcast
python WriteSbatch.py -p haswell -r 32 -n 32 -b Scatter

python WriteSbatch.py -p knl -r 32 -n 64 -b P2P
python WriteSbatch.py -p knl -r 32 -n 32 -b Broadcast
python WriteSbatch.py -p knl -r 32 -n 32 -b Scatter