    y[0]=0;
    for (i=1; i<m; i++){
        y[i]=y[i-1];
        for (j=0; j<n; j++){
            y[i]= y[i] + A[i][j]*x[j];
        }
    }
