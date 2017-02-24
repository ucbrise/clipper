package data;

import java.util.ArrayList;
import java.util.List;

public abstract class DataVectorParser<U, T extends DataVector<U>> {

    abstract List<U> parseBytes(byte[] bytes);

    abstract T constructDataVector(List<U> data);

    public List<T> parse(byte[] bytes, List<Integer> splits) {
        List<T> dataVectors = new ArrayList<>();
        List<U> data = parseBytes(bytes);
        if(splits.size() == 0) {
            T dataVector = constructDataVector(data);
            dataVectors.add(dataVector);
        } else {
            int prevSplit = 0;
            for(int currSplit : splits) {
                List<U> currData = new ArrayList<U>(data.subList(prevSplit, currSplit));
                T currVector = constructDataVector(currData);
                dataVectors.add(currVector);
                prevSplit = currSplit;
            }
        }
        return dataVectors;
    }

}
