load ../Output/OneSidedBusySend_Async_V2.txt
load ../Output/OneSidedBusySend_Sync.txt

data = [OneSidedBusySend_Async_V2 OneSidedBusySend_Sync] * 1000000;

boxplot(data);
ylim([0 5]);
