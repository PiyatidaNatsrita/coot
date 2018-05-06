
ylim=30
xlim=50

prediction_text_x_placement = 18
prediction_text_y_placement = 10

legend_x = 10
legend_x = 20
legend_y =  2.5


source('arrow.r')

predict = function(x_pos, y_pos) {

   scope_start = a$V3[1]
   scope_end   = a$V3[length(a$V3)]

   done_start = a$V2[1]
   done_end   = a$V2[length(a$V2)]

   s = paste(scope_start, scope_end)
   # print(s)
   s = paste(done_start, done_end)
   # print(s)

   now_day = a$V1[length(a$V1)]

   # print(now_day)

   c1 = done_start
   c2 = scope_start
   m1 = (done_end - done_start)/now_day
   m2 = (scope_end - scope_start)/now_day
   
   m_diff = m1 - m2
   if (m_diff > 0) {
      X_pred = (c2-c1)/(m1-m2)
      Y_pred_1 = m1 * X_pred + c1
      Y_pred_2 = m2 * X_pred + c2
      r = paste(X_pred+1, Y_pred_1)
      r = paste(r, now_day)
      # print(r)
      days_delta = X_pred - now_day
      today_t = Sys.Date()
      predict_t = today_t + days_delta
      date_s = format(predict_t, format="%d %B %Y")
      t = paste('Projected Release Day:\n', date_s)
      text(x_pos, y_pos, t, pos=3, cex=0.8)
      s = 3 # should depend on xlim 3 is good when xlim is 200
      s_x = 0.02*xlim
      s_y = 0.02*ylim
      rect(X_pred-s_x, Y_pred_1-s_y, X_pred+s_x, Y_pred_1+s_y, col = 'darkgreen')
      # need list of X values, list of Y values
      # not x,y pairs
      # lines(c(X_pred,X_pred), c(0, Y_pred_1), col='darkgreen', lty=2)
      lines(c(0,X_pred), c(0, Y_pred_1), col='darkgreen', lty=2)
      lines(c(0,X_pred), c(c2, Y_pred_1), col='darkgreen', lty=2)

      return(r)
   } else {
      print('bad m_diff')
      print(m_diff)
   }
}


a = read.table('burn-up.tab')


# it's tricky to change the plot resolution (sigh)
# png('burn-up.png', res=480, pointsize=8)
png('burn-up.png')

do_plot = function() {
   plot(ylim=c(0,ylim), xlim=c(0,xlim), a$V1, a$V2, t='l', lwd=2,
               main="Coot-0.8.9.2 Bug-Fix Development Progress",
               xlab="Days (since development start)",
               ylab="Dev Points (aka 'Half-Days')")
   points(a$V1, a$V3, t='l', lwd=2, lty=2)

   leg.txt <- c("Done", "Scope")
   legend(legend_x, legend_y, legend=leg.txt, lty=1:2, lwd=2, cex=0.7)
}

# text(175, 175, labels="CSHL Purge", col='grey', cex=0.7)
# arrows(160, 180, 118, 200, code=2, cex=0.5)
# betterArrow(160, 180, 118, 200, col='grey', code=2)

do_plot()

predict(prediction_text_x_placement, prediction_text_y_placement)

dev.off()

