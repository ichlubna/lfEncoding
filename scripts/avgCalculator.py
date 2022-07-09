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

folders = ["output5", "output12", "output19", "output26", "output33", "output40", "output47"]
files = ["metricsClassic.csv", "deepdecodedClassic.csv", "interpolationMetricsdecodedClassic.csv", "metricsAllKey.csv",  "deepdecodedAllKey.csv", "interpolationMetricsdecodedAllKey.csv", "metrics.csv", "deepdecoded.csv", "interpolationMetricsdecoded.csv"]
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
	print(str(res[1])+"\t"+str(res[2])+"\t"+str(res[0]))

print("blend and size")
for folder in folders:
	path=sys.argv[1]+"/"+folder+"/misc.csv"
	csvFile = list(csv.reader(open(path)))
	print(str(csvFile[2][1])+"\t"+str(csvFile[2][0])+"\t"+str(csvFile[3][1])+"\t"+str(csvFile[3][0])+"\t"+str(csvFile[1][1])+"\t"+str(csvFile[1][0])+"\t"+"s"+"\t"+str(csvFile[12][0])+"\t"+str(csvFile[14][0])+"\t"+str(int(csvFile[6][0])+int(csvFile[8][0])+int(csvFile[10][0])))
