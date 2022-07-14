import sys
import csv

#https://stackoverflow.com/a/25597628/3027604
def get_averages(csv):
    column_sums = None
    with open(csv) as file:
        lines = file.readlines()
        rows_of_numbers = [map(float, line.split(',')) for line in lines]
        sums = map(sum, zip(*rows_of_numbers))
        averages = [sum_item / len(lines) for sum_item in sums]
        return averages

folders = ["output0", "output7", "output14", "output21", "output28", "output35", "output42", "output49"]
files = ["metricsclassic.csv", "deepclassic.csv", "interpolationMetricsclassic.csv", "metricsclassic30.csv", "deepclassic30.csv", "interpolationMetricsclassic30.csv", "metricsclassicAllKey.csv",  "deepclassicAllKey.csv", "interpolationMetricsclassicAllKey.csv", "metricsproposed.csv", "deepproposed.csv", "interpolationMetricsproposed.csv"]
for file in files:
    print(file)
    for folder in folders:
        path=sys.argv[1]+"/"+folder+"/"+file
        res = get_averages(path)
        print(str(res[1])+"\t"+str(res[0]))

print("times")
for folder in folders:
    path=sys.argv[1]+"/"+folder+"/times.csv"
    res = get_averages(path)
    count=0
    for row in open(path):
        count+=1
    print(str(int(res[3])-int(res[4])-int(res[5])) +"\t"+ str(int(res[6])-int(res[7])-int(res[8])) +"\t"+ str(int(res[9])-int(res[10])) +"\t"+ str(int(res[0])+(int(res[1])/count)-int(res[2])))

print("blend and size")
for folder in folders:
    path=sys.argv[1]+"/"+folder+"/misc.csv"
    csvFile = list(csv.reader(open(path)))
    print(str(csvFile[2][1])+"\t"+str(csvFile[2][0])+"\t"+str(csvFile[3][1])+"\t"+str(csvFile[3][0])+"\t"+str(csvFile[4][1])+"\t"+str(csvFile[4][0])+"\t"+str(csvFile[1][1])+"\t"+str(csvFile[1][0])+"\t"+"s"+"\t"+str(csvFile[13][0])+"\t"+str(csvFile[15][0])+"\t"+str(csvFile[17][0])+"\t"+"\t"+str(int(csvFile[7][0])+int(csvFile[9][0])+int(csvFile[11][0])))

