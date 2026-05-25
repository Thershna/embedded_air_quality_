clc;
clear;
close all;
rng(7);

% Load data
data = readtable('chennai_pollution.csv');

PM25 = data.PM2_5;
CO   = data.CO;
NO2  = data.NO2;
AQI  = data.AQIValue;

% Remove missing
valid = ~isnan(PM25) & ~isnan(CO) & ~isnan(NO2) & ~isnan(AQI);

PM25 = PM25(valid);
CO   = CO(valid);
NO2  = NO2(valid);
AQI  = AQI(valid);

% Polynomial Features
X_poly = [ ...
    PM25 ...
    CO ...
    NO2 ...
    PM25.^2 ...
    CO.^2 ...
    NO2.^2 ...
    PM25.*CO ...
    PM25.*NO2 ...
    CO.*NO2 ...
];

Y = AQI;

% Train Test Split
cv = cvpartition(size(X_poly,1),'HoldOut',0.2);

Xtrain = X_poly(training(cv),:);
Ytrain = Y(training(cv));

Xtest = X_poly(test(cv),:);
Ytest = Y(test(cv));

%% Polynomial Regression
mdl = fitlm(Xtrain,Ytrain);
Ypred1 = predict(mdl,Xtest);

%% Residual Learning Random Forest (IMPORTANT: 50 TREES)
residual = Ytrain - predict(mdl,Xtrain);

t = templateTree('MinLeafSize',10);

rf = fitrensemble(Xtrain,residual,...
    'Method','Bag',...
    'NumLearningCycles',50,...
    'Learners',t);

Ypred2 = predict(rf,Xtest);

%% Final prediction
Ypred = Ypred1 + Ypred2;

rmse = sqrt(mean((Ytest - Ypred).^2));
R2 = 1 - sum((Ytest - Ypred).^2)/sum((Ytest - mean(Ytest)).^2);

disp(['RMSE = ' num2str(rmse)])
disp(['R^2 = ' num2str(R2)])

%% ================================
% DISPLAY BEST 5 TREES FOR ESP32
%% ================================
disp(' ');
disp('===== RESIDUAL TREES FOR ESP32 =====');

for i = 1:5
    fprintf('\n------------ TREE %d ------------\n', i);
    view(rf.Trained{i}, 'Mode', 'text');
end

%% ================================
% PLOT 1 : Actual vs Predicted
%% ================================
figure;
scatter(Ytest, Ypred, 'filled')
hold on
plot([min(Ytest) max(Ytest)], [min(Ytest) max(Ytest)], 'r', 'LineWidth',2)
xlabel('Actual AQI')
ylabel('Predicted AQI')
title('Actual vs Predicted AQI')
grid on

%% ================================
% PLOT 2 : Error Plot
%% ================================
figure;
plot(Ytest - Ypred)
xlabel('Sample')
ylabel('Prediction Error')
title('Prediction Error')
grid on

%% ================================
% PLOT 3 : Comparison Plot
%% ================================
figure;
plot(Ytest,'b','LineWidth',1.5)
hold on
plot(Ypred,'r','LineWidth',1.5)
legend('Actual AQI','Predicted AQI')
title('Actual vs Predicted Comparison')
grid on
mdl.Coefficients
