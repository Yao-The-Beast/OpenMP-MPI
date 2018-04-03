%% Load Sync
matrix_sync = [];
x_labels_sync = [];
throughputs_sync = [];

directory = '../Output/';
files = dir(strcat(directory, 'twoSidedBusySend_Multi_MPIs_Sync_*.txt'));
for i=1:length(files)
    thisFile = strcat(directory, files(i).name);
    % load file
    data = load(thisFile);
    % extract number (x label)
    this_x = str2double(regexp(files(i).name,'\d+','match'));
    % append x to the last row of the data
    data = [this_x; data];
    matrix_sync = [matrix_sync data];   
end

% sort the matrix based on the last row
sorted_matrix_sync = (sortrows(matrix_sync'))';

% extract x labels, throughputs
for i = 1 : size(sorted_matrix_sync, 2)
    x_labels_sync = [x_labels_sync sorted_matrix_sync(1, i)];
    throughputs_sync = [throughputs_sync sorted_matrix_sync(2, i)];
end

% remove the first and last row
sorted_matrix_sync(1,:) = [];
sorted_matrix_sync(1,:) = [];

% convert the data to microsecond
sorted_matrix_sync = sorted_matrix_sync * 1000000;


%% Load Async
matrix_async = [];
x_labels_async = [];
throughputs_async = [];

directory = '../Output/';
files = dir(strcat(directory, 'twoSidedBusySend_Multi_MPIs_Async_*.txt'));
for i=1:length(files)
    thisFile = strcat(directory, files(i).name);
    % load file
    data = load(thisFile);
    % extract number (x label)
    this_x = str2double(regexp(files(i).name,'\d+','match'));
    % append x to the last row of the data
    data = [this_x; data];
    matrix_async = [matrix_async data];   
end

% sort the matrix based on the last row
sorted_matrix_async = (sortrows(matrix_async'))';

% extract x labels, throughputs
for i = 1 : size(sorted_matrix_async, 2)
    x_labels_async = [x_labels_async sorted_matrix_async(1, i)];
    throughputs_async = [throughputs_async sorted_matrix_async(2, i)];
end

% remove the first and last row
sorted_matrix_async(1,:) = [];
sorted_matrix_async(1,:) = [];

% convert the data to microsecond
sorted_matrix_async = sorted_matrix_async * 1000000;


%% Append two matrix into one, to do side by side comparison
new_matrix = [];
new_labels = {};
for i = 1:size(x_labels_async, 2)
    new_matrix = [new_matrix sorted_matrix_sync(:, i) sorted_matrix_async(:, i)];
    new_labels{1, end+1} = strcat('S ', num2str(x_labels_sync(:, i))); 
    new_labels{1, end+1} = strcat('A ', num2str(x_labels_sync(:, i))); 
end;


boxplot(new_matrix,'Notch','on','Labels',new_labels);
ylim([0 5]);
set(gcf,'units','points','position',[100,100,1000,500]);
set(gca,'fontsize',12);
ylabel('Latency in ms');
xlabel('A: Async, S: Sync, # : Number of Running MPIs');

